"""Controlled local Ollama experiment; standard library only."""
import argparse
import json
import time
import urllib.request
from pathlib import Path

QUESTIONS = [
    {
        "id": "q1",
        "question": "Как запустить тесты? Укажи файл-источник.",
    },
    {
        "id": "q2",
        "question": "Что будет при пустом имени подписчика? Подтверди кодом.",
    },
    {
        "id": "q3",
        "question": "Где реализован unsubscribe? Проверь предпосылку вопроса.",
    },
    {
        "id": "q4",
        "question": "Какая CI-система запускает тесты? Если сведений нет, скажи об этом.",
    },
    {
        "id": "q5",
        "question": "Сохраняются ли подписки после перезапуска процесса? Подтверди кодом.",
    },
]

def build_repo_context(demo_dir: Path) -> str:
    parts = []
    for filename in ["README.md", "Makefile", "service.py", "test_service.py"]:
        fpath = demo_dir / filename
        if fpath.exists():
            parts.append(f"--- File: {filename} ---\n{fpath.read_text(encoding='utf-8')}")
    return "\n\n".join(parts)

def run_query(model: str, messages: list, temperature: float, seed: int) -> tuple[dict, dict]:
    payload = {
        "model": model,
        "messages": messages,
        "stream": False,
        "think": False,
        "options": {
            "temperature": temperature,
            "seed": seed,
            "num_ctx": 4096,
            "num_predict": 512,
        },
    }
    request = urllib.request.Request(
        "http://localhost:11434/api/chat",
        data=json.dumps(payload).encode("utf-8"),
        headers={"Content-Type": "application/json"},
    )
    started = time.perf_counter()
    with urllib.request.urlopen(request, timeout=300) as response:
        answer = json.load(response)
    duration = answer.get("eval_duration", 0)
    record = {
        "request": payload,
        "response": answer,
        "wall_seconds": time.perf_counter() - started,
        "load_seconds": answer.get("load_duration", 0) / 1e9,
        "total_seconds": answer.get("total_duration", 0) / 1e9,
        "eval_count": answer.get("eval_count", 0),
        "prompt_eval_count": answer.get("prompt_eval_count", 0),
        "decode_tokens_per_second": answer.get("eval_count", 0) / (duration / 1e9) if duration else None,
    }
    return payload, record

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--mode", choices=["baseline", "system", "all"], default="all")
    p.add_argument("--model", default="itmo-local")
    p.add_argument("--temperature", type=float, default=0.2)
    p.add_argument("--seed", type=int, default=42)
    p.add_argument("--output-dir", default=None)
    args = p.parse_args()

    root = Path(__file__).resolve().parent
    demo_dir = root / "demo"
    context = build_repo_context(demo_dir)
    results_dir = Path(args.output_dir) if args.output_dir else root / "results"
    results_dir.mkdir(parents=True, exist_ok=True)

    system_prompt = (root / "system.txt").read_text(encoding="utf-8")

    all_results = []
    for item in QUESTIONS:
        qid = item["id"]
        qtext = item["question"]
        print(f"\n=== Executing {qid}: {qtext} ===")
        messages = []
        if args.mode in ("system", "all"):
            messages.append({"role": "system", "content": system_prompt})
        user_content = f"Контекст репозитория:\n{context}\n\nВопрос:\n{qtext}"
        messages.append({"role": "user", "content": user_content})

        _, record = run_query(args.model, messages, args.temperature, args.seed)
        ans_text = record["response"].get("message", {}).get("content", "").strip()
        print(f"Ответ:\n{ans_text}")
        print(f"Скорость: {record['decode_tokens_per_second']:.2f} tok/s, Время: {record['wall_seconds']:.2f}s")

        out_file = results_dir / f"{qid}.json"
        with out_file.open("w", encoding="utf-8") as f:
            json.dump(record, f, ensure_ascii=False, indent=2)

        all_results.append({
            "id": qid,
            "question": qtext,
            "answer": ans_text,
            "tokens_per_second": record["decode_tokens_per_second"],
            "wall_seconds": record["wall_seconds"],
            "eval_count": record["eval_count"],
        })

    summary_file = results_dir / "summary.json"
    with summary_file.open("w", encoding="utf-8") as f:
        json.dump(all_results, f, ensure_ascii=False, indent=2)
    print(f"\nВсе 5 проверок завершены. Результаты сохранены в {results_dir}")

if __name__ == "__main__":
    main()


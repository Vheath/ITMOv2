#!/usr/bin/env python3
"""
Lightweight Fast JSON-RPC MCP server for SDL3 documentation, header symbols, and headers.
Stdio JSON-RPC 2.0 compliant.
"""
import sys
import json
import os
import glob
import re

HEADERS_DIR = "/nix/store/vq7xnwxwil95kf6p2sjqxl87gfl7kqjg-sdl3-3.4.10-dev/include/SDL3"
if not os.path.exists(HEADERS_DIR):
    candidates = glob.glob("/nix/store/*-sdl3-*-dev/include/SDL3")
    if candidates:
        HEADERS_DIR = candidates[0]

def list_headers():
    if not os.path.isdir(HEADERS_DIR):
        return []
    return sorted([f for f in os.listdir(HEADERS_DIR) if f.endswith(".h")])

def search_symbols(query: str, max_results: int = 15):
    query_lower = query.lower()
    matches = []
    if not os.path.isdir(HEADERS_DIR):
        return matches
    for fname in os.listdir(HEADERS_DIR):
        if not fname.endswith(".h"):
            continue
        fpath = os.path.join(HEADERS_DIR, fname)
        try:
            with open(fpath, "r", encoding="utf-8", errors="ignore") as f:
                for line_no, line in enumerate(f, 1):
                    if query_lower in line.lower() and ("SDL_" in line or "extern " in line or "typedef " in line or "#define " in line):
                        matches.append({
                            "header": fname,
                            "line": line_no,
                            "content": line.strip()
                        })
                        if len(matches) >= max_results:
                            return matches
        except Exception:
            pass
    return matches

def get_header_definition(header_name: str, symbol: str):
    fpath = os.path.join(HEADERS_DIR, header_name if header_name.endswith(".h") else f"{header_name}.h")
    if not os.path.exists(fpath):
        return f"Header {header_name} not found"
    
    with open(fpath, "r", encoding="utf-8", errors="ignore") as f:
        lines = f.readlines()

    target_idx = -1
    for i, line in enumerate(lines):
        if symbol in line:
            target_idx = i
            break

    if target_idx == -1:
        return f"Symbol {symbol} not found in {header_name}"

    start = max(0, target_idx - 15)
    end = min(len(lines), target_idx + 25)
    return "".join(lines[start:end])

TOOLS = [
    {
        "name": "sdl3_list_headers",
        "description": "List all installed SDL3 C header files available in include/SDL3",
        "inputSchema": {
            "type": "object",
            "properties": {},
            "additionalProperties": False
        }
    },
    {
        "name": "sdl3_search_symbol",
        "description": "Search SDL3 functions, structs, enums, and macros in installed SDL3 headers",
        "inputSchema": {
            "type": "object",
            "properties": {
                "query": {
                    "type": "string",
                    "description": "Function name, enum, or type (e.g. SDL_CreateWindow, SDL_Renderer, SDL_PollEvent)"
                },
                "max_results": {
                    "type": "integer",
                    "description": "Maximum number of results to return (default: 15)"
                }
            },
            "required": ["query"],
            "additionalProperties": False
        }
    },
    {
        "name": "sdl3_get_definition",
        "description": "Get declaration and doc-comments around a symbol inside an SDL3 header",
        "inputSchema": {
            "type": "object",
            "properties": {
                "header": {
                    "type": "string",
                    "description": "Header file name (e.g. SDL_video.h or SDL_render.h)"
                },
                "symbol": {
                    "type": "string",
                    "description": "Symbol name (e.g. SDL_CreateWindow)"
                }
            },
            "required": ["header", "symbol"],
            "additionalProperties": False
        }
    }
]

def handle_rpc(request):
    method = request.get("method")
    req_id = request.get("id")

    if method == "initialize":
        return {
            "jsonrpc": "2.0",
            "id": req_id,
            "result": {
                "protocolVersion": "2024-11-05",
                "capabilities": {
                    "tools": {}
                },
                "serverInfo": {
                    "name": "sdl3-mcp-server",
                    "version": "1.0.0"
                }
            }
        }
    elif method == "notifications/initialized":
        return None
    elif method == "tools/list":
        return {
            "jsonrpc": "2.0",
            "id": req_id,
            "result": {
                "tools": TOOLS
            }
        }
    elif method == "tools/call":
        params = request.get("params", {})
        tool_name = params.get("name")
        args = params.get("arguments", {})

        if tool_name == "sdl3_list_headers":
            headers = list_headers()
            return {
                "jsonrpc": "2.0",
                "id": req_id,
                "result": {
                    "content": [{"type": "text", "text": json.dumps(headers, indent=2)}]
                }
            }
        elif tool_name == "sdl3_search_symbol":
            query = args.get("query", "")
            limit = int(args.get("max_results", 15))
            res = search_symbols(query, limit)
            return {
                "jsonrpc": "2.0",
                "id": req_id,
                "result": {
                    "content": [{"type": "text", "text": json.dumps(res, indent=2)}]
                }
            }
        elif tool_name == "sdl3_get_definition":
            header = args.get("header", "")
            symbol = args.get("symbol", "")
            snippet = get_header_definition(header, symbol)
            return {
                "jsonrpc": "2.0",
                "id": req_id,
                "result": {
                    "content": [{"type": "text", "text": snippet}]
                }
            }
        else:
            return {
                "jsonrpc": "2.0",
                "id": req_id,
                "error": {
                    "code": -32601,
                    "message": f"Method {tool_name} not found"
                }
            }
    elif method == "ping":
        return {"jsonrpc": "2.0", "id": req_id, "result": {}}
    else:
        if req_id is not None:
            return {
                "jsonrpc": "2.0",
                "id": req_id,
                "error": {
                    "code": -32601,
                    "message": f"Unhandled method: {method}"
                }
            }
        return None

def main():
    while True:
        line = sys.stdin.readline()
        if not line:
            break
        line = line.strip()
        if not line:
            continue
        
        # Check if HTTP-style header (Content-Length)
        if line.lower().startswith("content-length:"):
            try:
                length = int(line.split(":")[1].strip())
                # read empty line
                sys.stdin.readline()
                body = sys.stdin.read(length)
                req = json.loads(body)
            except Exception as e:
                continue
        else:
            # Line-delimited JSON
            try:
                req = json.loads(line)
            except Exception:
                continue

        response = handle_rpc(req)
        if response is not None:
            # opencode MCP client expects line-delimited JSON or standard JSON-RPC
            sys.stdout.write(json.dumps(response) + "\n")
            sys.stdout.flush()

if __name__ == "__main__":
    main()

"""Security regression tests against the vulnerabilities documented in REPORT_01.

Each test asserts the FIXED behaviour. Running the same file against the original
vulnerable service.py (git HEAD) makes these tests fail, which demonstrates that
they detect the problems before the fix.
"""

import json
import threading
import unittest
from http.server import ThreadingHTTPServer
from urllib.error import HTTPError
from urllib.request import Request, urlopen

from sandbox import Sandbox
from service import Shop, make_handler


class SecurityRegressionTest(unittest.TestCase):
    def setUp(self):
        self.shop = Shop()
        self.addCleanup(self.shop.close)

    def request(self, method, path, body=None, headers=None):
        with self.shop.lock:
            return self.shop.dispatch(method, path, body or {}, headers or {})

    def login(self, username="alice", password="learn-alice"):
        status, result = self.request("POST", "/login", {"username": username, "password": password})
        self.assertEqual(status, 200)
        return {"X-Token": result["token"]}

    # REPORT_01 #1: SQLi in /profile?id=
    def test_sqli_in_profile_is_blocked(self):
        auth = self.login()
        status, result = self.request("GET", "/profile?id=1 OR 1=1", headers=auth)
        self.assertNotEqual(status, 200)
        self.assertNotIsInstance(result, list)

    # REPORT_01 #2: SQLi in /search?q=
    def test_sqli_in_search_is_blocked(self):
        status, result = self.request("GET", "/search?q=' OR '1'='1")
        self.assertEqual(status, 200)
        self.assertEqual(result, [])

    # REPORT_01 #3: SQLi in /assistant/context?order_id=
    def test_sqli_in_assistant_context_is_blocked(self):
        auth = self.login()
        status, result = self.request("GET", "/assistant/context?order_id=1 OR 1=1", headers=auth)
        self.assertNotEqual(status, 200)

    # REPORT_01 #4: /profile must require login and own profile
    def test_profile_requires_login(self):
        status, _ = self.request("GET", "/profile?id=1")
        self.assertEqual(status, 401)

    def test_profile_hides_other_users(self):
        auth = self.login()
        status, result = self.request("GET", "/profile?id=2", headers=auth)
        self.assertEqual(status, 403)
        self.assertNotIn("TEST_BOB", json.dumps(result))

    # REPORT_01 #5: /orders must require login and be scoped to the owner
    def test_orders_require_login(self):
        status, _ = self.request("GET", "/orders")
        self.assertEqual(status, 401)

    def test_orders_scoped_to_owner(self):
        auth = self.login()
        status, result = self.request("GET", "/orders", headers=auth)
        self.assertEqual(status, 200)
        self.assertTrue(all(order["owner"] == 1 for order in result))

    # REPORT_01 #6: role must come from the server session, not X-Role
    def test_admin_export_ignores_role_header(self):
        _, result = self.request("GET", "/admin/export", headers={"X-Role": "staff"})
        self.assertEqual(result, {"error": "login required"})

    def test_admin_export_staff_only_via_session(self):
        auth = self.login("alice", "learn-alice")
        status, result = self.request("GET", "/admin/export", headers={**auth, "X-Role": "staff"})
        self.assertEqual(status, 403)
        self.assertNotIn("TEST_ALICE", json.dumps(result))

    def test_staff_export_works_with_real_session(self):
        auth = self.login("staff", "learn-staff")
        status, profiles = self.request("GET", "/admin/export", headers=auth)
        self.assertEqual(status, 200)
        self.assertEqual(len(profiles), 2)

    # REPORT_01 #7: price must come from the catalog, not the client
    def test_price_tampering_is_rejected(self):
        auth = self.login()
        status, order = self.request("POST", "/orders", {"product": "chair", "price": 1}, auth)
        self.assertEqual(status, 201)
        self.assertEqual(order["price"], 5000)

    # REPORT_01 #8: logical path traversal inside sandbox is blocked
    def test_private_note_is_blocked_for_customers(self):
        auth = self.login()
        status, result = self.request("GET", "/files?user=..&path=private/operator-note.txt", headers=auth)
        self.assertEqual(status, 403)
        self.assertNotIn("TEST_ONLY_OPERATOR_SECRET", json.dumps(result))

    # REPORT_01 #9: IDOR, customers cannot read each other's receipts
    def test_other_users_receipt_is_blocked(self):
        auth = self.login()
        status, result = self.request("GET", "/files?user=bob&path=receipt.txt", headers=auth)
        self.assertEqual(status, 403)
        self.assertNotIn("TEST_BOB", json.dumps(result))

    # REPORT_01 #10: stored XSS through reviews is escaped
    def test_review_xss_is_escaped(self):
        self.request("POST", "/reviews", {"product": "chair", "text": "<script>alert(1)</script>"})
        status, page = self.request("GET", "/reviews/view")
        self.assertEqual(status, 200)
        self.assertIn("&lt;script&gt;", page)
        self.assertNotIn("<script>alert(1)</script>", page)

    # REPORT_01 #11: logs must not contain passwords or live tokens
    def test_debug_logs_redact_credentials(self):
        token = self.request("POST", "/login", {"username": "alice", "password": "learn-alice"})[1]["token"]
        status, logs = self.request("GET", "/debug/logs")
        self.assertEqual(status, 200)
        dump = json.dumps(logs)
        self.assertNotIn("learn-alice", dump)
        self.assertNotIn(token, dump)

    # REPORT_01 #12: assistant executor requires login and enforces tool rights
    def test_assistant_execute_requires_login(self):
        status, result = self.request("POST", "/assistant/execute",
                                      {"decision": {"tool": "read_file", "arguments": {"path": "files/alice/receipt.txt"}}})
        self.assertEqual(status, 401)

    def test_assistant_read_other_users_file_is_blocked(self):
        auth = self.login()
        status, result = self.request("POST", "/assistant/execute",
                                      {"decision": {"tool": "read_file", "arguments": {"path": "files/bob/receipt.txt"}}}, auth)
        self.assertEqual(status, 403)
        self.assertNotIn("TEST_BOB", json.dumps(result))

    def test_assistant_export_profiles_staff_only(self):
        auth = self.login()
        status, result = self.request("POST", "/assistant/execute",
                                      {"decision": {"tool": "export_profiles"}}, auth)
        self.assertEqual(status, 403)
        self.assertNotIn("TEST_ALICE", json.dumps(result))

    def test_assistant_delete_tree_staff_only(self):
        auth = self.login()
        status, result = self.request("POST", "/assistant/execute",
                                      {"decision": {"tool": "delete_tree", "arguments": {"path": "."}}}, auth)
        self.assertEqual(status, 403)
        self.assertEqual(result.get("deleted", None), None)
        # files must still exist for the owner
        status, receipt = self.request("GET", "/files?user=alice&path=receipt.txt", headers=auth)
        self.assertEqual(status, 200)
        self.assertIn("TEST_ALICE", receipt["content"])

    def test_staff_can_delete_inside_sandbox(self):
        auth = self.login("staff", "learn-staff")
        status, result = self.request("POST", "/assistant/execute",
                                      {"decision": {"tool": "delete_tree", "arguments": {"path": "files/alice"}}}, auth)
        self.assertEqual(status, 200)
        self.assertEqual(result, {"deleted": "files/alice"})


class HttpErrorHandlingTest(unittest.TestCase):
    """REPORT_01 #14: HTTP 500 responses must not leak internal paths."""

    @classmethod
    def setUpClass(cls):
        cls.shop = Shop()
        cls.server = ThreadingHTTPServer(("127.0.0.1", 0), make_handler(cls.shop))
        cls.server.daemon_threads = True
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()
        cls.base = f"http://127.0.0.1:{cls.server.server_port}"

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.thread.join(timeout=5)
        cls.server.server_close()
        cls.shop.close()

    def call(self, path, headers=None):
        request = Request(self.base + path, headers=headers or {})
        try:
            with urlopen(request, timeout=5) as response:
                return response.status, response.read().decode()
        except HTTPError as exc:
            return exc.status, exc.read().decode()

    def login(self):
        request = Request(
            self.base + "/login",
            data=json.dumps({"username": "alice", "password": "learn-alice"}).encode(),
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        with urlopen(request, timeout=5) as response:
            return {"X-Token": json.loads(response.read())["token"]}

    def test_internal_error_does_not_leak_paths(self):
        headers = self.login()
        status, body = self.call("/files?user=alice&path=missing.txt", headers=headers)
        self.assertEqual(status, 500)
        self.assertIn("internal error", body)
        self.assertNotIn("/tmp/", body)
        self.assertNotIn("FileNotFoundError", body)


if __name__ == "__main__":
    unittest.main()
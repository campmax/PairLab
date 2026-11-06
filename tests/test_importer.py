
import importlib.util, json, unittest
from datetime import date, timedelta, datetime, timezone
from unittest.mock import patch
from pathlib import Path

spec = importlib.util.spec_from_file_location(
    "fetch_data", Path(__file__).resolve().parents[1] / "tools/fetch_data.py"
)
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)


class Response:
    def __init__(self, payload):
        self.payload = payload

    def __enter__(self):
        return self

    def __exit__(self, *args):
        pass

    def read(self, *args):
        return json.dumps(self.payload).encode()


def fixture():
    dates = [date(2010, 1, 1) + timedelta(days=i) for i in range(800)]
    timestamps = [
        int(datetime.combine(d, datetime.min.time(), timezone.utc).timestamp())
        for d in dates
    ]
    result = {
        "meta": {"currency": "USD", "dataGranularity": "1d"},
        "timestamp": timestamps,
        "indicators": {
            "quote": [{k: [50.0] * 800 for k in ["open", "high", "low", "close"]}]
        },
        "events": {
            "splits": {
                "s": {"date": timestamps[700], "numerator": 2, "denominator": 1}
            },
            "dividends": {"d": {"date": timestamps[600], "amount": 0.5}},
        },
    }
    return {"chart": {"result": [result]}}, dates


class ImportTests(unittest.TestCase):
    def test_future_split_is_undone_before_crop(self):
        p, d = fixture()
        with patch.object(m.urllib.request, "urlopen", return_value=Response(p)):
            rows, _ = m.fetch_chart("TEST", d[0], d[650])
            self.assertEqual(rows[0][1], 100)
            self.assertEqual(rows[600][5], 1)
            self.assertEqual(rows[0][-1], 1)

    def test_split_date_uses_new_units(self):
        p, d = fixture()
        with patch.object(m.urllib.request, "urlopen", return_value=Response(p)):
            rows, _ = m.fetch_chart("TEST", d[0], d[-1] + timedelta(days=1))
            self.assertEqual(rows[699][1], 100)
            self.assertEqual(rows[700][1], 50)
            self.assertEqual(rows[700][-1], 2)

    def test_rejects_downsampled_response(self):
        p, d = fixture()
        p["chart"]["result"][0]["meta"]["dataGranularity"] = "3mo"
        with patch.object(
            m.urllib.request, "urlopen", return_value=Response(p)
        ), self.assertRaises(ValueError):
            m.fetch_chart("TEST", d[0], d[-1])

    def test_rejects_missing_price(self):
        p, d = fixture()
        p["chart"]["result"][0]["indicators"]["quote"][0]["close"][2] = None
        with patch.object(
            m.urllib.request, "urlopen", return_value=Response(p)
        ), self.assertRaises((ValueError, TypeError)):
            m.fetch_chart("TEST", d[0], d[-1])

    def test_duplicate_dates_rejected(self):
        p, d = fixture()
        p["chart"]["result"][0]["timestamp"][2] = p["chart"]["result"][0]["timestamp"][
            1
        ]
        with patch.object(
            m.urllib.request, "urlopen", return_value=Response(p)
        ), self.assertRaises(ValueError):
            m.fetch_chart("TEST", d[0], d[-1])

    def test_fingerprint_known_value(self):
        self.assertEqual(m.fnv(b"hello"), "a430d84680aabd0b")


if __name__ == "__main__":
    unittest.main()

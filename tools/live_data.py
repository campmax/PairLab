pass                                                                       
import json, math, time, urllib.parse, urllib.request
from datetime import datetime, timezone


def iso_day(timestamp):
    return datetime.fromtimestamp(timestamp, timezone.utc).date().isoformat()


def normalize(result, now):
    meta = result['meta']
    if meta.get('currency') != 'USD' or meta.get('dataGranularity') != '1d':
        raise ValueError('Expected USD daily history')
    price = float(meta['regularMarketPrice'])
    timestamp = float(meta['regularMarketTime'])
    if not all(math.isfinite(v) and v > 0 for v in (price, timestamp)):
        raise ValueError('Invalid latest trade')
    day = iso_day(timestamp)
    timestamps = result['timestamp']
    dates = [iso_day(t) for t in timestamps]
    if dates != sorted(set(dates)):
        raise ValueError('Duplicate or unordered daily history')
    divs = {}
    for kind in ('dividends', 'capitalGains'):
        for event in result.get('events', {}).get(kind, {}).values():
            d = iso_day(event['date'])
            amount = float(event['amount'])
            if not math.isfinite(amount) or amount < 0:
                raise ValueError('Invalid distribution')
            divs[d] = divs.get(d, 0) + amount
    closes = result['indicators']['quote'][0]['close']
    if len(closes) != len(dates):
        raise ValueError('Malformed history lengths')
    history = []
    for d, close in zip(dates, closes):
        if d >= day:
            continue                                                         
        if close is None or not math.isfinite(float(close)) or float(close) <= 0:
            raise ValueError('Missing completed close; no silent filling')
        history.append({'date': d, 'close': float(close), 'dividend': divs.get(d, 0)})
    if any(d not in dates for d in divs if history and history[0]['date'] <= d < day):
        raise ValueError('Distribution without a matching price session')
    regular = meta['currentTradingPeriod']['regular']
    return {'currency': 'USD', 'basis': 'current_split_units', 'price': price,
            'quote_time': timestamp, 'quote_date': day, 'received_at': now,
            'session_open': float(regular['start']), 'session_close': float(regular['end']),
            'current_dividend': divs.get(day, 0), 'history': history}


def fetch(symbol):
    if not symbol or any(c not in 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-' for c in symbol):
        raise ValueError('Invalid symbol')
    now = time.time()
    query = urllib.parse.urlencode({'period1': int(now - 1100 * 86400), 'period2': int(now),
                                    'interval': '1d', 'events': 'div,splits'})
    request = urllib.request.Request('https://query1.finance.yahoo.com/v8/finance/chart/' + symbol + '?' + query,
                                     headers={'User-Agent': 'Mozilla/5.0'})
    with urllib.request.urlopen(request, timeout=20) as response:
        data = json.load(response)
    result = data['chart']['result']
    if not result:
        raise ValueError('Provider returned no data')
    return normalize(result[0], time.time())

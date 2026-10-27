---
schema_version: 1
wo_id: WO-16.1
pr_id: PR-16
title: WebSocket Registration Frame, Keepalive, Fallback Watchdog & Rendering Fixes
complexity: standard
status: COMPLETED
allowed_files:
- web/ride.js
- web/echenon-dashboard.js
forbidden_files:
- src/**
- include/**
- framework/**
- platformio.ini
required_skills: []
verification_command: pio test -e native --verbose
max_attempts: 3
max_structural_failures: 2
spec_hash: null
completion:
  status: COMPLETED
  verification_summary: pio test -e native --verbose — 233/233 PASSED
---

# WO-16.1 — WebSocket Registration Frame, Keepalive, Fallback Watchdog & Rendering Fixes

> **PR:** PR-16 (WebSocket Client Registration Fix)
> **Complexity:** standard
> **Verification:** `pio test -e native --verbose` (233/233 must pass)

---

## 1. Objective

Fix the WebSocket live telemetry blackout caused by ESP-IDF v5's internal
101-handshake behavior. Both `web/ride.js` and `web/echenon-dashboard.js`
must send a registration data frame after `ws.onopen` to trigger
`PsychicWebSocketHandler::checkForNewClient()` and register the socket in
`_clients`. Additionally, add keepalive pings and a fallback HTTP watchdog.

Also fix three rendering bugs in ride history display:
- Dashboard date field mismatch (`ride.date` → `ride.start_epoch_s`)
- Dashboard `fetchLastRide()` silent failure (add console.warn in catch)
- Investigate `/ride` page "No rides yet" despite API returning data

---

## 2. Strict Boundary Box

### 2.1 Allowed Files (exhaustive)

| File | Changes |
|---|---|
| `web/ride.js` | Registration frame, lastRx watchdog, keepalive interval, Strava link, investigate "No rides" rendering |
| `web/echenon-dashboard.js` | Registration frame, lastWsRx watchdog, keepalive interval, fallback fetch, fix date field mismatch, add catch logging |

### 2.2 Forbidden

- **All C++ source/header files** — the server-side code is correct.
- **platformio.ini** — no build config changes needed.
- **framework/** — no framework changes.

---

## 3. Implementation Specification

### 3.1 `web/ride.js` Changes

**A. Registration frame in `ws.onopen` (line 22):**

Current:
```js
w.onopen=function(){$('ws-dot').className='conn on';rt=1e3};
```

Change to:
```js
w.onopen=function(){$('ws-dot').className='conn on';rt=1e3;try{w.send('ping')}catch(e){}};
```

**B. Add `lastRx` state variable (line 2 area):**

Add `lastRx=0` to the existing `var` declaration line:
```js
var w,rt=1e3,el=0,st='IDLE',spd=0,dst=0,ap=0,pc=0,lastRx=0;
```

**C. Track `lastRx` in `w.onmessage` (line 24):**

Current:
```js
w.onmessage=function(e){try{tl(JSON.parse(e.data))}catch(x){}};
```

Change to:
```js
w.onmessage=function(e){lastRx=Date.now();try{tl(JSON.parse(e.data))}catch(x){}};
```

**D. Update `pl()` polling fallback (lines 26-29):**

Current:
```js
function pl(){
if(!w||w.readyState!==1){
fetch('/api/echenon/live').then(function(r){return r.json()}).then(function(d){tl(d)}).catch(function(){});
}}
```

Change to:
```js
function pl(){
if(!w||w.readyState!==1||(lastRx>0&&(Date.now()-lastRx)>2500)){
fetch('/api/echenon/live').then(function(r){return r.json()}).then(function(d){tl(d)}).catch(function(){});
}}
```

**E. Add 15s keepalive interval (line 88 area):**

After `ws();` call on line 88, add a keepalive:
```js
setInterval(function(){if(w&&w.readyState===1)try{w.send('ping')}catch(e){}},15e3);
```

**F. Add Strava activity link in `lh()` (line 74):**

Current (line 74):
```js
h+='<div class="r"><span>'+dt+' · '+dur+' · '+((r.avg_power_w||0))+'W</span><a href="/api/session/download?id='+r.session_id+'">TCX</a></div>';
```

Change to:
```js
h+='<div class="r"><span>'+dt+' · '+dur+' · '+((r.avg_power_w||0))+'W</span><a href="/api/session/download?id='+r.session_id+'">TCX</a>'+(r.activity_id?'<a href="https://www.strava.com/activities/'+r.activity_id+'" target="_blank">Strava ↗</a>':'')+'</div>';
```

### 3.2 `web/echenon-dashboard.js` Changes

**A. Registration frame in `ws.onopen` (line 72-75):**

Current:
```js
    ws.onopen = function() {
      wsReconnectDelay = 1000;
      updateWsStatus('connected', 'Live');
    };
```

Change to:
```js
    ws.onopen = function() {
      wsReconnectDelay = 1000;
      updateWsStatus('connected', 'Live');
      try { ws.send('ping'); } catch(e) {}
    };
```

**B. Add `lastWsRx` state variable (line 14 area):**

After `var MAX_RECONNECT_DELAY = 16000;` add:
```js
  var lastWsRx = 0;
```

**C. Track `lastWsRx` in `ws.onmessage` (line 77):**

Current:
```js
    ws.onmessage = function(e) {
      try {
```

Change to:
```js
    ws.onmessage = function(e) {
      lastWsRx = Date.now();
      try {
```

**D. Add 15s keepalive interval inside `connectWS()` (after ws.onclose block, before end of function):**

Before the closing `}` of `connectWS()`, add:
```js
    // 15s keepalive ping
    var keepalive = setInterval(function() {
      if (!ws || ws.readyState !== WebSocket.OPEN) { clearInterval(keepalive); return; }
      try { ws.send('ping'); } catch(e) { clearInterval(keepalive); }
    }, 15000);
```

**E. Add fallback HTTP fetch watchdog:**

In the `tab-changed` event handler, add a periodic check. After the `fetchAll(); connectWS();` calls:
```js
      // Fallback: if WS stalls, fetch via HTTP
      if (!window._echenonFallback) {
        window._echenonFallback = setInterval(function() {
          if (isTabActive && lastWsRx > 0 && (Date.now() - lastWsRx) > 2500) {
            fetchAll();
          }
        }, 2500);
      }
```

---

## 4. Rendering Bug Fixes (Additive Scope)

### 4.1 Dashboard Date Field Mismatch (Bug 1 — CONFIRMED)

**File:** `web/echenon-dashboard.js`, `fetchLastRide()`, lines 376-378

The API `/api/session/history` returns `start_epoch_s` (epoch seconds), but the
code checks `ride.date || ride.started_at || ride.timestamp` — none of which
exist in the response. This silently produces `dateStr = '-'`.

**Current (lines 375-379):**
```js
      var dateStr = '-';
      if (ride.date || ride.started_at || ride.timestamp) {
        var d = new Date(ride.date || ride.started_at || ride.timestamp);
        dateStr = d.toLocaleDateString(undefined, { month: 'short', day: 'numeric', year: 'numeric' });
      }
```

**Change to:**
```js
      var dateStr = '-';
      if (ride.start_epoch_s) {
        var d = new Date(ride.start_epoch_s * 1000);
        dateStr = d.toLocaleDateString(undefined, { month: 'short', day: 'numeric', year: 'numeric' });
      } else if (ride.date || ride.started_at || ride.timestamp) {
        var d = new Date(ride.date || ride.started_at || ride.timestamp);
        dateStr = d.toLocaleDateString(undefined, { month: 'short', day: 'numeric', year: 'numeric' });
      }
```

### 4.2 Dashboard `fetchLastRide()` Silent Failure (Bug 3)

**File:** `web/echenon-dashboard.js`, `fetchLastRide()`, line 391

The empty `catch` block silently swallows errors, leaving the default "No ride
history available" text even when rides exist but parsing fails.

**Current (line 391):**
```js
    } catch (err) {
      // non-fatal — history endpoint may not be available yet
    }
```

**Change to:**
```js
    } catch (err) {
      console.warn('[echenon-dashboard] fetchLastRide error:', err);
    }
```

### 4.3 `/ride` Page "No Rides Yet" Investigation (Bug 2)

**File:** `web/ride.js`, `lh()` function

The code at line 72 correctly uses `r.start_epoch_s`, and the API returns rides.
The user saw "No rides yet" despite valid API data. **Investigate:**

1. Read `lh()` carefully. Check whether `rides.slice(-5)` might be empty if
   the response shape differs from expected.
2. Check whether `$('history')` element exists on the page.
3. If no code bug is found, the most likely cause is **stale PROGMEM assets** —
   the gzipped JS in `include/portal_assets/ride_js.hpp` was compiled from an
   older `web/ride.js`. The rebuild in WO-16.2 would resolve this.
4. **Document your finding** in Part II of this WORKORDER.md — either the code
   fix you made, or the conclusion that asset staleness is the root cause.

---

## 5. Verification Criteria

1. `pio test -e native --verbose` passes 233/233 (JS changes don't affect native C++ tests).
2. Both files are syntactically valid JavaScript (no parse errors when loaded by the browser).
3. The `ws.send('ping')` call appears inside the `onopen` handler of both files.
4. The keepalive interval (15s) is present in both files.
5. The `lastRx` / `lastWsRx` watchdog logic triggers HTTP fallback when no WS message received for >2.5s.
6. The Strava link renders conditionally on `r.activity_id` presence.
7. Dashboard `fetchLastRide()` uses `ride.start_epoch_s * 1000` for date conversion.
8. Dashboard `fetchLastRide()` catch block logs to `console.warn`.
9. Part II documents the Bug 2 investigation finding (code fix or asset staleness conclusion).
## Completion Report

_No completion report recorded._

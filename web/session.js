/**
 * Echenon Ride Session & History Tab
 *
 * Self-registering tab module for the ESP32Modular portal framework.
 * Listens for 'tab-changed' events with detail.tab === 'tab-session'.
 *
 * Endpoints consumed:
 *   GET  /api/session/status
 *   POST /api/session/start
 *   POST /api/session/pause
 *   POST /api/session/resume
 *   POST /api/session/end
 *   POST /api/session/discard
 *   POST /api/session/config
 *   GET  /api/session/history
 *   GET  /api/session/download?id=<id>[&gz=1]
 *   POST /api/session/upload?id=<id>
 */
(function () {
    'use strict';

    var TAB_ID = 'tab-session';
    var POLL_MS = 2000;

    var initialized = false;
    var pollTimer = null;

    var stylesInjected = false;
    function injectStyles() {
        if (stylesInjected) return;
        stylesInjected = true;
        var style = document.createElement('style');
        style.textContent = [
            '.ech-sess-grid {',
            '  display: grid;',
            '  grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));',
            '  gap: 0.75rem;',
            '  margin-bottom: 1rem;',
            '}',
            '.ech-sess-stat {',
            '  background: var(--bg-input, #0f172a);',
            '  border: 1px solid var(--border, #334155);',
            '  border-radius: 0.5rem;',
            '  padding: 0.75rem;',
            '  text-align: center;',
            '}',
            '.ech-sess-stat-label {',
            '  font-size: 0.7rem;',
            '  text-transform: uppercase;',
            '  letter-spacing: 0.06em;',
            '  color: var(--text-muted, #94a3b8);',
            '  margin-bottom: 0.25rem;',
            '}',
            '.ech-sess-stat-val {',
            '  font-size: 1.35rem;',
            '  font-weight: 700;',
            '  font-variant-numeric: tabular-nums;',
            '}',
            '.ech-sess-controls {',
            '  display: flex;',
            '  flex-wrap: wrap;',
            '  gap: 0.5rem;',
            '  margin-bottom: 1rem;',
            '}',
            '.ech-sess-badge {',
            '  display: inline-block;',
            '  padding: 0.2rem 0.6rem;',
            '  border-radius: 999px;',
            '  font-size: 0.75rem;',
            '  font-weight: 700;',
            '}',
            '.ech-sess-badge-idle { background: rgba(148,163,184,0.15); color: #94a3b8; }',
            '.ech-sess-badge-active { background: rgba(34,197,94,0.15); color: #22c55e; }',
            '.ech-sess-badge-paused { background: rgba(245,158,11,0.15); color: #f59e0b; }',
            '.ech-sess-badge-ended { background: rgba(59,130,246,0.15); color: #3b82f6; }',
            '.ech-sess-table {',
            '  width: 100%;',
            '  border-collapse: collapse;',
            '  font-size: 0.85rem;',
            '}',
            '.ech-sess-table th, .ech-sess-table td {',
            '  padding: 0.5rem;',
            '  border-bottom: 1px solid var(--border, #334155);',
            '  text-align: left;',
            '}',
            '.ech-sess-actions {',
            '  display: flex;',
            '  flex-wrap: wrap;',
            '  gap: 0.35rem;',
            '}',
            '.ech-sess-btn-sm {',
            '  padding: 0.25rem 0.5rem;',
            '  font-size: 0.75rem;',
            '  border-radius: 0.35rem;',
            '  text-decoration: none;',
            '  cursor: pointer;',
            '}'
        ].join('\n');
        document.head.appendChild(style);
    }

    function formatDuration(sec) {
        var s = Math.max(0, Math.floor(Number(sec) || 0));
        var h = Math.floor(s / 3600);
        var m = Math.floor((s % 3600) / 60);
        var rem = s % 60;
        var mm = (m < 10 ? '0' : '') + m;
        var ss = (rem < 10 ? '0' : '') + rem;
        return h > 0 ? (h + ':' + mm + ':' + ss) : (mm + ':' + ss);
    }

    function formatEpoch(epoch) {
        var n = Number(epoch) || 0;
        if (n < 1700000000) return 'Unsynced (' + n + 's)';
        return new Date(n * 1000).toLocaleString();
    }

    function buildUI(container) {
        injectStyles();
        container.innerHTML = [
            '<div class="card">',
            '  <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:0.75rem;">',
            '    <h2 style="margin:0;">Ride Session</h2>',
            '    <span id="ech-sess-state" class="ech-sess-badge ech-sess-badge-idle">IDLE</span>',
            '  </div>',
            '  <div class="ech-sess-grid">',
            '    <div class="ech-sess-stat">',
            '      <div class="ech-sess-stat-label">Elapsed</div>',
            '      <div id="ech-sess-elapsed" class="ech-sess-stat-val">00:00</div>',
            '    </div>',
            '    <div class="ech-sess-stat">',
            '      <div class="ech-sess-stat-label">Avg Power</div>',
            '      <div id="ech-sess-avg-w" class="ech-sess-stat-val">0 W</div>',
            '    </div>',
            '    <div class="ech-sess-stat">',
            '      <div class="ech-sess-stat-label">Max Power</div>',
            '      <div id="ech-sess-max-w" class="ech-sess-stat-val">0 W</div>',
            '    </div>',
            '    <div class="ech-sess-stat">',
            '      <div class="ech-sess-stat-label">Total Work</div>',
            '      <div id="ech-sess-work-kj" class="ech-sess-stat-val">0 kJ</div>',
            '    </div>',
            '  </div>',
            '  <div class="ech-sess-controls">',
            '    <button id="ech-btn-start" class="btn btn-primary" type="button">Start Ride</button>',
            '    <button id="ech-btn-pause" class="btn btn-secondary" type="button">Pause</button>',
            '    <button id="ech-btn-resume" class="btn btn-primary" type="button">Resume</button>',
            '    <button id="ech-btn-end" class="btn btn-primary" type="button">End &amp; Save</button>',
            '    <button id="ech-btn-discard" class="btn btn-danger" type="button">Discard</button>',
            '  </div>',
            '  <div id="ech-sess-msg" style="font-size:0.85rem;color:var(--text-muted,#94a3b8);min-height:1.2em;"></div>',
            '</div>',
            '<div class="card">',
            '  <h2>Session Settings</h2>',
            '  <div style="display:flex;flex-wrap:wrap;gap:1rem;align-items:flex-end;">',
            '    <label style="display:flex;align-items:center;gap:0.5rem;">',
            '      <input type="checkbox" id="ech-cfg-autostart" checked>',
            '      <span>Auto-Start on Cadence</span>',
            '    </label>',
            '    <div>',
            '      <label for="ech-cfg-delay" style="display:block;font-size:0.75rem;color:var(--text-muted,#94a3b8);">Auto-Start Delay (s)</label>',
            '      <input type="number" id="ech-cfg-delay" min="1" max="60" value="3" style="width:5rem;">',
            '    </div>',
            '    <div>',
            '      <label for="ech-cfg-timeout" style="display:block;font-size:0.75rem;color:var(--text-muted,#94a3b8);">Pause Timeout (s)</label>',
            '      <input type="number" id="ech-cfg-timeout" min="30" max="3600" value="300" style="width:6rem;">',
            '    </div>',
            '    <button id="ech-btn-save-cfg" class="btn btn-secondary" type="button">Save Settings</button>',
            '  </div>',
            '</div>',
            '<div class="card">',
            '  <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:0.75rem;">',
            '    <h2 style="margin:0;">Stored Rides</h2>',
            '    <button id="ech-btn-refresh-history" class="btn btn-secondary ech-sess-btn-sm" type="button">Refresh</button>',
            '  </div>',
            '  <div style="overflow-x:auto;">',
            '    <table class="ech-sess-table">',
            '      <thead>',
            '        <tr>',
            '          <th>ID</th>',
            '          <th>Started</th>',
            '          <th>Duration</th>',
            '          <th>Avg / Max W</th>',
            '          <th>Work</th>',
            '          <th>Upload</th>',
            '          <th>Actions</th>',
            '        </tr>',
            '      </thead>',
            '      <tbody id="ech-sess-history-body">',
            '        <tr><td colspan="7" style="text-align:center;color:var(--text-muted,#94a3b8);">Loading...</td></tr>',
            '      </tbody>',
            '    </table>',
            '  </div>',
            '</div>'
        ].join('\n');

        document.getElementById('ech-btn-start').addEventListener('click', function () {
            postAction('/api/session/start');
        });
        document.getElementById('ech-btn-pause').addEventListener('click', function () {
            postAction('/api/session/pause');
        });
        document.getElementById('ech-btn-resume').addEventListener('click', function () {
            postAction('/api/session/resume');
        });
        document.getElementById('ech-btn-end').addEventListener('click', function () {
            postAction('/api/session/end', true);
        });
        document.getElementById('ech-btn-discard').addEventListener('click', function () {
            postAction('/api/session/discard', true);
        });
        document.getElementById('ech-btn-save-cfg').addEventListener('click', saveConfig);
        document.getElementById('ech-btn-refresh-history').addEventListener('click', fetchHistory);
    }

    function setMessage(msg) {
        var el = document.getElementById('ech-sess-msg');
        if (el) el.textContent = msg || '';
    }

    function postAction(url, refreshHistoryAfter) {
        fetch(url, { method: 'POST', credentials: 'same-origin' })
            .then(function (r) { return r.json(); })
            .then(function (data) {
                if (data && data.error) {
                    setMessage('Error: ' + data.error);
                } else {
                    setMessage('');
                }
                fetchStatus();
                if (refreshHistoryAfter) fetchHistory();
            })
            .catch(function (err) {
                setMessage('Request failed: ' + err);
            });
    }

    function saveConfig() {
        var autoStart = document.getElementById('ech-cfg-autostart').checked;
        var delay = parseInt(document.getElementById('ech-cfg-delay').value, 10) || 3;
        var timeout = parseInt(document.getElementById('ech-cfg-timeout').value, 10) || 300;
        fetch('/api/session/config', {
            method: 'POST',
            credentials: 'same-origin',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                auto_start_enabled: autoStart,
                auto_start_delay_sec: delay,
                inactivity_timeout_sec: timeout
            })
        })
            .then(function (r) { return r.json(); })
            .then(function () {
                setMessage('Session settings saved.');
                fetchStatus();
            })
            .catch(function (err) {
                setMessage('Failed to save settings: ' + err);
            });
    }

    var configPopulated = false;
    function updateStatusUI(data) {
        if (!data) return;
        var state = (data.state || 'IDLE').toUpperCase();
        var badge = document.getElementById('ech-sess-state');
        if (badge) {
            badge.textContent = state;
            badge.className = 'ech-sess-badge ech-sess-badge-' + state.toLowerCase();
        }
        var elElapsed = document.getElementById('ech-sess-elapsed');
        if (elElapsed) elElapsed.textContent = formatDuration(data.elapsed_s || 0);

        var avgW = data.avg_power_w !== undefined ? data.avg_power_w : (data.avg_watts || 0);
        var maxW = data.max_power_w !== undefined ? data.max_power_w : (data.max_watts || 0);
        var workKj = data.total_work_kj !== undefined ? data.total_work_kj : (data.calories_kcal || 0);

        var elAvg = document.getElementById('ech-sess-avg-w');
        if (elAvg) elAvg.textContent = Math.round(avgW) + ' W';
        var elMax = document.getElementById('ech-sess-max-w');
        if (elMax) elMax.textContent = Math.round(maxW) + ' W';
        var elWork = document.getElementById('ech-sess-work-kj');
        if (elWork) elWork.textContent = Math.round(workKj) + ' kJ';

        if (!configPopulated) {
            var chk = document.getElementById('ech-cfg-autostart');
            if (chk && data.auto_start_enabled !== undefined) chk.checked = !!data.auto_start_enabled;
            var dly = document.getElementById('ech-cfg-delay');
            if (dly && data.auto_start_delay_sec !== undefined) dly.value = data.auto_start_delay_sec;
            var tmo = document.getElementById('ech-cfg-timeout');
            if (tmo && data.inactivity_timeout_sec !== undefined) tmo.value = data.inactivity_timeout_sec;
            configPopulated = true;
        }
    }

    function fetchStatus() {
        fetch('/api/session/status', { credentials: 'same-origin' })
            .then(function (r) { return r.json(); })
            .then(updateStatusUI)
            .catch(function () {});
    }

    function renderHistory(data) {
        var tbody = document.getElementById('ech-sess-history-body');
        if (!tbody) return;
        var rides = (data && data.rides) ? data.rides : [];
        if (rides.length === 0) {
            tbody.innerHTML = '<tr><td colspan="7" style="text-align:center;color:var(--text-muted,#94a3b8);">No stored rides</td></tr>';
            return;
        }
        var rows = [];
        for (var i = 0; i < rides.length; i++) {
            var r = rides[i];
            var id = r.session_id;
            var dur = formatDuration(r.active_duration_s || r.duration_s || 0);
            var uploadState = r.upload_state || 'NONE';
            rows.push(
                '<tr>' +
                '<td>#' + id + '</td>' +
                '<td>' + formatEpoch(r.start_epoch_s) + '</td>' +
                '<td>' + dur + '</td>' +
                '<td>' + (r.avg_power_w || 0) + ' / ' + (r.max_power_w || 0) + ' W</td>' +
                '<td>' + (r.total_work_kj || 0) + ' kJ</td>' +
                '<td>' + uploadState + '</td>' +
                '<td class="ech-sess-actions">' +
                '<a class="btn btn-secondary ech-sess-btn-sm" href="/api/session/download?id=' + id + '&format=tcx" download>TCX</a>' +
                '<a class="btn btn-secondary ech-sess-btn-sm" href="/api/session/download?id=' + id + '&format=tcx.gz&gz=1" download>GZ</a>' +
                '<button class="btn btn-primary ech-sess-btn-sm" data-upload-id="' + id + '" type="button">Upload</button>' +
                '<button class="btn btn-danger ech-sess-btn-sm" data-delete-id="' + id + '" type="button">Delete</button>' +
                '</td>' +
                '</tr>'
            );
        }
        tbody.innerHTML = rows.join('');

        var uploadBtns = tbody.querySelectorAll('[data-upload-id]');
        for (var u = 0; u < uploadBtns.length; u++) {
            uploadBtns[u].addEventListener('click', function (ev) {
                var sid = ev.currentTarget.getAttribute('data-upload-id');
                postAction('/api/session/upload?id=' + encodeURIComponent(sid), true);
            });
        }
        var delBtns = tbody.querySelectorAll('[data-delete-id]');
        for (var d = 0; d < delBtns.length; d++) {
            delBtns[d].addEventListener('click', function (ev) {
                var sid = ev.currentTarget.getAttribute('data-delete-id');
                postAction('/api/session/discard?id=' + encodeURIComponent(sid), true);
            });
        }
    }

    function fetchHistory() {
        fetch('/api/session/history', { credentials: 'same-origin' })
            .then(function (r) { return r.json(); })
            .then(renderHistory)
            .catch(function () {});
    }

    function startPolling() {
        if (pollTimer) clearInterval(pollTimer);
        fetchStatus();
        fetchHistory();
        pollTimer = setInterval(fetchStatus, POLL_MS);
    }

    function stopPolling() {
        if (pollTimer) {
            clearInterval(pollTimer);
            pollTimer = null;
        }
    }

    function onTabChanged(e) {
        var targetTab = e.detail && e.detail.tab;
        if (targetTab === TAB_ID) {
            var container = document.getElementById(TAB_ID);
            if (!container) return;
            if (!initialized) {
                buildUI(container);
                initialized = true;
            }
            startPolling();
        } else {
            stopPolling();
        }
    }

    document.addEventListener('tab-changed', onTabChanged);

    var el = document.getElementById(TAB_ID);
    if (el && el.classList.contains('active')) {
        buildUI(el);
        initialized = true;
        startPolling();
    }
})();

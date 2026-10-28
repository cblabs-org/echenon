/**
 * Echenon Apps — Intervals.icu API Key Configuration
 *
 * Pill panel under Apps tab.
 * Manages Intervals.icu API credentials, connection testing, and auto-sync.
 *
 * Endpoints consumed:
 *   GET  /api/intervals/status
 *   POST /api/intervals/credentials
 *   POST /api/intervals/test
 *   POST /api/intervals/disconnect
 *   POST /api/session/config
 */
(function () {
    'use strict';

    var TAB_ID = 'tab-intervals-icu';
    var initialized = false;

    function buildUI(container) {
        container.innerHTML = [
            '<div style="background:var(--card-bg); border:1px solid var(--border, #e0e0e0); border-radius:16px; padding:1.25rem; margin-bottom:1rem;">',
            '  <h4 style="margin:0 0 0.5rem 0; font-size:1rem; font-weight:700; color:var(--text, #212121);">API Credentials</h4>',
            '  <p style="font-size:0.8rem;color:var(--text-muted);margin:0 0 0.75rem 0; line-height:1.5;">',
            '    Enter your Intervals.icu Athlete ID and API Key. Find these at ',
            '    <a href="https://intervals.icu/settings" target="_blank" rel="noopener" style="color:var(--primary, #2196F3);">intervals.icu/settings</a> ',
            '    under &ldquo;Developer&rdquo;. The API key is stored encrypted and is write-only.',
            '  </p>',
            '  <div style="display:grid;gap:0.75rem;max-width:420px;">',
            '    <div>',
            '      <label for="ech-intervals-athlete-id" style="display:block;font-size:0.8rem;font-weight:600;margin-bottom:0.25rem;color:var(--text-muted);">Athlete ID</label>',
            '      <input type="text" id="ech-intervals-athlete-id" placeholder="e.g. i12345" autocomplete="off" style="width:100%;padding:8px 10px;border:1px solid var(--border, #e0e0e0);border-radius:10px;font-size:0.9rem;color:var(--text, #212121);background:var(--card-bg);">',
            '    </div>',
            '    <div>',
            '      <label for="ech-intervals-api-key" style="display:block;font-size:0.8rem;font-weight:600;margin-bottom:0.25rem;color:var(--text-muted);">API Key (write-only)</label>',
            '      <input type="password" id="ech-intervals-api-key" placeholder="Enter new API key" autocomplete="new-password" style="width:100%;padding:8px 10px;border:1px solid var(--border, #e0e0e0);border-radius:10px;font-size:0.9rem;color:var(--text, #212121);background:var(--card-bg);">',
            '    </div>',
            '    <div>',
            '      <button id="ech-btn-intervals-save" type="button" style="padding:8px 20px; font-size:0.85rem; font-weight:700; border:none; border-radius:10px; background:var(--primary, #2196F3); color:#fff; cursor:pointer;">Save Credentials</button>',
            '    </div>',
            '  </div>',
            '  <div id="ech-intervals-msg" style="font-size:0.85rem;color:var(--text-muted);margin-top:0.75rem;min-height:1.2em;"></div>',
            '</div>',
            '<div style="background:var(--card-bg); border:1px solid var(--border, #e0e0e0); border-radius:16px; padding:1.25rem;">',
            '  <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:0.75rem;">',
            '    <h3 style="margin:0; font-size:1.15rem; font-weight:700; color:var(--text, #212121);">Intervals.icu Integration</h3>',
            '    <span id="ech-intervals-badge" style="display:inline-block; padding:2px 10px; font-size:0.75rem; font-weight:700; border-radius:20px; background:var(--border, #e0e0e0); color:var(--text-muted);">Checking...</span>',
            '  </div>',
            '  <p style="margin:0 0 0.75rem 0; font-size:0.85rem; color:var(--text-muted); line-height:1.5;">',
            '    Connect your Intervals.icu account to automatically upload completed rides.',
            '  </p>',
            '  <div id="ech-intervals-details" style="font-size:0.85rem;color:var(--text-muted);margin-bottom:1rem;"></div>',
            '  <div style="display:flex;flex-wrap:wrap;gap:0.5rem;align-items:center;margin-bottom:0.75rem;">',
            '    <button id="ech-btn-intervals-test" type="button" style="padding:8px 16px; font-size:0.85rem; font-weight:700; border:none; border-radius:10px; background:var(--primary, #2196F3); color:#fff; cursor:pointer;">Test Connection</button>',
            '    <button id="ech-btn-intervals-disconnect" type="button" style="padding:8px 16px; font-size:0.85rem; font-weight:600; border:1px solid var(--danger, #c62828); border-radius:10px; background:transparent; color:var(--danger, #c62828); cursor:pointer;">Disconnect</button>',
            '    <button id="ech-btn-intervals-refresh" type="button" style="padding:8px 16px; font-size:0.85rem; font-weight:600; border:1px solid var(--border, #e0e0e0); border-radius:10px; background:transparent; color:var(--text, #212121); cursor:pointer;">Refresh Status</button>',
            '  </div>',
            '  <label style="display:flex;align-items:center;gap:0.5rem;margin-top:0.5rem;font-size:0.85rem;color:var(--text, #212121);">',
            '    <input type="checkbox" id="ech-intervals-autoupload" checked>',
            '    <span>Auto-upload completed rides to Intervals.icu</span>',
            '  </label>',
            '</div>'
        ].join('\n');

        document.getElementById('ech-btn-intervals-save').addEventListener('click', saveCredentials);
        document.getElementById('ech-btn-intervals-test').addEventListener('click', testConnection);
        document.getElementById('ech-btn-intervals-disconnect').addEventListener('click', disconnectIntervals);
        document.getElementById('ech-btn-intervals-refresh').addEventListener('click', fetchIntervalsStatus);
        document.getElementById('ech-intervals-autoupload').addEventListener('change', saveAutoUpload);
    }

    function setMsg(text) {
        var el = document.getElementById('ech-intervals-msg');
        if (el) el.textContent = text || '';
    }

    function updateStatusUI(data) {
        if (!data) return;
        var badge = document.getElementById('ech-intervals-badge');
        var details = document.getElementById('ech-intervals-details');
        var athleteIdInput = document.getElementById('ech-intervals-athlete-id');

        if (badge) {
            if (data.authenticated) {
                badge.textContent = 'CONNECTED';
                badge.style.background = 'rgba(46, 125, 50, 0.15)';
                badge.style.color = 'var(--success, #2e7d32)';
            } else if (data.configured) {
                badge.textContent = 'CREDENTIALS SAVED';
                badge.style.background = 'rgba(249, 168, 37, 0.15)';
                badge.style.color = '#f9a825';
            } else {
                badge.textContent = 'NOT CONFIGURED';
                badge.style.background = 'var(--border, #e0e0e0)';
                badge.style.color = 'var(--text-muted)';
            }
        }

        if (details) {
            var parts = [];
            if (data.athlete_name) parts.push('Athlete: ' + data.athlete_name);
            if (data.athlete_id) parts.push('Athlete ID: ' + data.athlete_id);
            details.textContent = parts.join(' | ');
        }

        if (athleteIdInput && data.athlete_id && !athleteIdInput.value) {
            athleteIdInput.value = data.athlete_id;
        }

        var autoChk = document.getElementById('ech-intervals-autoupload');
        if (autoChk && data.enabled !== undefined) {
            autoChk.checked = !!data.enabled;
        }
    }

    function fetchIntervalsStatus() {
        fetch('/api/intervals/status', { credentials: 'same-origin' })
            .then(function (r) { return r.json(); })
            .then(updateStatusUI)
            .catch(function () {});
    }

    function saveCredentials() {
        var athleteId = (document.getElementById('ech-intervals-athlete-id').value || '').trim();
        var keyEl = document.getElementById('ech-intervals-api-key');
        var apiKey = (keyEl.value || '').trim();
        if (!athleteId || !apiKey) {
            setMsg('Both Athlete ID and API Key are required.');
            return;
        }
        fetch('/api/intervals/credentials', {
            method: 'POST',
            credentials: 'same-origin',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ athlete_id: athleteId, api_key: apiKey })
        })
            .then(function (r) { return r.json(); })
            .then(function (data) {
                if (data && data.ok) {
                    keyEl.value = '';
                    setMsg('Credentials saved to encrypted storage.');
                    fetchIntervalsStatus();
                } else {
                    setMsg('Failed: ' + ((data && data.error) || 'Unknown error'));
                }
            })
            .catch(function (err) {
                setMsg('Error saving credentials: ' + err);
            });
    }

    function testConnection() {
        setMsg('Testing connection...');
        fetch('/api/intervals/test', {
            method: 'POST',
            credentials: 'same-origin'
        })
            .then(function (r) { return r.json(); })
            .then(function (data) {
                if (data && data.ok) {
                    setMsg('Connection test passed.');
                    fetchIntervalsStatus();
                } else {
                    setMsg('Connection test failed: ' + ((data && data.error) || ((data && data.message) || 'Unknown error')));
                }
            })
            .catch(function (err) {
                setMsg('Error testing connection: ' + err);
            });
    }

    function disconnectIntervals() {
        fetch('/api/intervals/disconnect', { method: 'POST', credentials: 'same-origin' })
            .then(function (r) { return r.json(); })
            .then(function () {
                setMsg('Disconnected from Intervals.icu.');
                fetchIntervalsStatus();
            })
            .catch(function (err) {
                setMsg('Error disconnecting: ' + err);
            });
    }

    function saveAutoUpload() {
        var enabled = document.getElementById('ech-intervals-autoupload').checked;
        fetch('/api/session/config', {
            method: 'POST',
            credentials: 'same-origin',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ auto_upload_enabled: enabled })
        })
            .then(function (r) { return r.json(); })
            .then(function () {
                setMsg('Auto-upload preference updated.');
            })
            .catch(function () {});
    }

    function onTabChanged(e) {
        var detail = e.detail;
        var targetTab = (detail && detail.tab) ? detail.tab : detail;
        if (targetTab === TAB_ID) {
            var container = document.getElementById(TAB_ID);
            if (!container) return;
            if (!initialized) {
                buildUI(container);
                initialized = true;
            }
            fetchIntervalsStatus();
        }
    }

    document.addEventListener('tab-changed', onTabChanged);
    window.addEventListener('tab-changed', onTabChanged);

    var el = document.getElementById(TAB_ID);
    if (el && el.classList.contains('active')) {
        buildUI(el);
        initialized = true;
        fetchIntervalsStatus();
    }
})();

/**
 * Echenon Upload Services (Strava OAuth2 & Auto-Upload) Tab
 *
 * Self-registering tab module for the ESP32Modular portal framework.
 * Listens for 'tab-changed' events with detail.tab === 'tab-upload-services'.
 *
 * Endpoints consumed:
 *   GET  /api/strava/status
 *   POST /api/strava/credentials
 *   GET  /api/strava/auth-url
 *   POST /api/strava/disconnect
 *   POST /api/session/config
 */
(function () {
    'use strict';

    var TAB_ID = 'tab-upload-services';
    var initialized = false;

    function buildUI(container) {
        container.innerHTML = [
            '<div class="card">',
            '  <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:0.75rem;">',
            '    <h2 style="margin:0;">Strava Integration</h2>',
            '    <span id="ech-strava-badge" style="font-weight:700;font-size:0.85rem;">Checking...</span>',
            '  </div>',
            '  <div id="ech-strava-details" style="font-size:0.85rem;color:var(--text-muted,#94a3b8);margin-bottom:1rem;"></div>',
            '  <div style="display:flex;flex-wrap:wrap;gap:0.5rem;margin-bottom:0.75rem;">',
            '    <button id="ech-btn-strava-auth" class="btn btn-primary" type="button">Authorize with Strava</button>',
            '    <button id="ech-btn-strava-disconnect" class="btn btn-danger" type="button">Disconnect Strava</button>',
            '    <button id="ech-btn-strava-refresh" class="btn btn-secondary" type="button">Refresh Status</button>',
            '  </div>',
            '  <label style="display:flex;align-items:center;gap:0.5rem;margin-top:0.5rem;">',
            '    <input type="checkbox" id="ech-strava-autoupload" checked>',
            '    <span>Auto-upload completed rides to Strava</span>',
            '  </label>',
            '</div>',
            '<div class="card">',
            '  <h2>Strava API Credentials</h2>',
            '  <p style="font-size:0.8rem;color:var(--text-muted,#94a3b8);margin-top:0;">',
            '    Enter your Strava API Application Client ID and Client Secret. The secret is stored encrypted in NVS and is write-only.',
            '  </p>',
            '  <div style="display:grid;gap:0.75rem;max-width:420px;">',
            '    <div>',
            '      <label for="ech-strava-client-id" style="display:block;font-size:0.75rem;margin-bottom:0.25rem;">Client ID</label>',
            '      <input type="text" id="ech-strava-client-id" placeholder="e.g. 12345" autocomplete="off" style="width:100%;">',
            '    </div>',
            '    <div>',
            '      <label for="ech-strava-client-secret" style="display:block;font-size:0.75rem;margin-bottom:0.25rem;">Client Secret (write-only)</label>',
            '      <input type="password" id="ech-strava-client-secret" placeholder="Enter new client secret" autocomplete="new-password" style="width:100%;">',
            '    </div>',
            '    <div>',
            '      <button id="ech-btn-strava-save" class="btn btn-primary" type="button">Save Credentials</button>',
            '    </div>',
            '  </div>',
            '  <div id="ech-strava-msg" style="font-size:0.85rem;color:var(--text-muted,#94a3b8);margin-top:0.75rem;min-height:1.2em;"></div>',
            '</div>'
        ].join('\n');

        document.getElementById('ech-btn-strava-save').addEventListener('click', saveCredentials);
        document.getElementById('ech-btn-strava-auth').addEventListener('click', startOAuth);
        document.getElementById('ech-btn-strava-disconnect').addEventListener('click', disconnectStrava);
        document.getElementById('ech-btn-strava-refresh').addEventListener('click', fetchStravaStatus);
        document.getElementById('ech-strava-autoupload').addEventListener('change', saveAutoUpload);
    }

    function setMsg(text) {
        var el = document.getElementById('ech-strava-msg');
        if (el) el.textContent = text || '';
    }

    function updateStatusUI(data) {
        if (!data) return;
        var badge = document.getElementById('ech-strava-badge');
        var details = document.getElementById('ech-strava-details');
        var clientIdInput = document.getElementById('ech-strava-client-id');

        if (badge) {
            if (data.authenticated) {
                badge.textContent = 'CONNECTED';
                badge.style.color = '#22c55e';
            } else if (data.configured) {
                badge.textContent = 'CREDENTIALS SAVED (NOT AUTHORIZED)';
                badge.style.color = '#f59e0b';
            } else {
                badge.textContent = 'NOT CONFIGURED';
                badge.style.color = '#94a3b8';
            }
        }

        if (details) {
            var parts = [];
            if (data.athlete_name) parts.push('Athlete: ' + data.athlete_name);
            if (data.client_id) parts.push('Client ID: ' + data.client_id);
            parts.push('Queue pending: ' + (data.pending_count || 0) + ' / ' + (data.queue_size || 0));
            details.textContent = parts.join(' | ');
        }

        if (clientIdInput && data.client_id && !clientIdInput.value) {
            clientIdInput.value = data.client_id;
        }

        var autoChk = document.getElementById('ech-strava-autoupload');
        if (autoChk && data.auto_upload !== undefined) {
            autoChk.checked = !!data.auto_upload;
        }
    }

    function fetchStravaStatus() {
        fetch('/api/strava/status', { credentials: 'same-origin' })
            .then(function (r) { return r.json(); })
            .then(updateStatusUI)
            .catch(function () {});
    }

    function saveCredentials() {
        var cid = (document.getElementById('ech-strava-client-id').value || '').trim();
        var secEl = document.getElementById('ech-strava-client-secret');
        var sec = (secEl.value || '').trim();
        if (!cid || !sec) {
            setMsg('Both Client ID and Client Secret are required.');
            return;
        }
        fetch('/api/strava/credentials', {
            method: 'POST',
            credentials: 'same-origin',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ client_id: cid, client_secret: sec })
        })
            .then(function (r) { return r.json(); })
            .then(function (data) {
                if (data && data.ok) {
                    secEl.value = '';
                    setMsg('Credentials saved to encrypted storage.');
                    fetchStravaStatus();
                } else {
                    setMsg('Failed: ' + ((data && data.error) || 'Unknown error'));
                }
            })
            .catch(function (err) {
                setMsg('Error saving credentials: ' + err);
            });
    }

    function startOAuth() {
        var redirectUri = window.location.origin + '/api/strava/callback';
        fetch('/api/strava/auth-url?redirect_uri=' + encodeURIComponent(redirectUri), {
            credentials: 'same-origin'
        })
            .then(function (r) { return r.json(); })
            .then(function (data) {
                var target = data && (data.url || data.authorize_url);
                if (target) {
                    window.location.href = target;
                } else {
                    setMsg('Could not build authorize URL: ' + ((data && data.error) || 'Save credentials first'));
                }
            })
            .catch(function (err) {
                setMsg('Failed to request auth URL: ' + err);
            });
    }

    function disconnectStrava() {
        fetch('/api/strava/disconnect', { method: 'POST', credentials: 'same-origin' })
            .then(function (r) { return r.json(); })
            .then(function () {
                setMsg('Disconnected from Strava.');
                fetchStravaStatus();
            })
            .catch(function (err) {
                setMsg('Error disconnecting: ' + err);
            });
    }

    function saveAutoUpload() {
        var enabled = document.getElementById('ech-strava-autoupload').checked;
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
        var targetTab = e.detail && e.detail.tab;
        if (targetTab === TAB_ID) {
            var container = document.getElementById(TAB_ID);
            if (!container) return;
            if (!initialized) {
                buildUI(container);
                initialized = true;
            }
            fetchStravaStatus();
        }
    }

    document.addEventListener('tab-changed', onTabChanged);

    var el = document.getElementById(TAB_ID);
    if (el && el.classList.contains('active')) {
        buildUI(el);
        initialized = true;
        fetchStravaStatus();
    }
})();

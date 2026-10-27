/**
 * Echenon Apps — Strava OAuth2 Configuration
 *
 * Pill panel under Apps tab.
 * Manages Strava API credentials and OAuth authorization.
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

    var TAB_ID = 'tab-strava';
    var initialized = false;

    function buildUI(container) {
        container.innerHTML = [
            '<div style="background:var(--card-bg); border:1px solid var(--border, #e0e0e0); border-radius:16px; padding:1.25rem; margin-bottom:1rem;">',
            '  <h4 style="margin:0 0 0.5rem 0; font-size:1rem; font-weight:700; color:var(--text, #212121);">Setup Guide</h4>',
            '  <ol style="margin:0; padding-left:1.25rem; font-size:0.85rem; color:var(--text-muted); line-height:1.6;">',
            '    <li>Go to <a href="https://www.strava.com/settings/api" target="_blank" rel="noopener" style="color:var(--primary, #2196F3);">strava.com/settings/api</a> and create an API application</li>',
            '    <li>Set <strong>Application Name</strong> to <em>Echenon</em> (or your preference), <strong>Category</strong> to <em>Data Importer</em></li>',
            '    <li><strong>Website</strong> (required): enter the callback URL shown below &mdash; <code id="ech-callback-url" style="font-family:monospace;"></code></li>',
            '    <li>Set <strong>Authorization Callback Domain</strong> to: <code id="ech-callback-domain" style="font-family:monospace; background:var(--border, #e0e0e0); padding:2px 6px; border-radius:4px;"></code></li>',
            '    <li><strong>Application Description</strong>: e.g. <em>Echelon Ex3 Bike Data Importer</em></li>',
            '    <li>After clicking <strong>Create</strong>, Strava will ask you to <strong>upload an app icon</strong> &mdash; any small image will do</li>',
            '    <li>Copy the <strong>Client ID</strong> and <strong>Client Secret</strong> from the app page, enter them below, then click <strong>Save Credentials</strong></li>',
            '    <li>Click <strong>Authorize with Strava</strong> to connect your account</li>',
            '  </ol>',
            '</div>',
            '<div style="background:var(--card-bg); border:1px solid var(--border, #e0e0e0); border-radius:16px; padding:1.25rem; margin-bottom:1rem;">',
            '  <h4 style="margin:0 0 0.5rem 0; font-size:1rem; font-weight:700; color:var(--text, #212121);">Strava API Credentials</h4>',
            '  <p style="font-size:0.8rem;color:var(--text-muted);margin:0 0 0.75rem 0; line-height:1.5;">',
            '    Enter your Strava API Application Client ID and Client Secret. The secret is stored encrypted in NVS and is write-only.',
            '  </p>',
            '  <div style="display:grid;gap:0.75rem;max-width:420px;">',
            '    <div>',
            '      <label for="ech-strava-client-id" style="display:block;font-size:0.8rem;font-weight:600;margin-bottom:0.25rem;color:var(--text-muted);">Client ID</label>',
            '      <input type="text" id="ech-strava-client-id" placeholder="e.g. 12345" autocomplete="off" style="width:100%;padding:8px 10px;border:1px solid var(--border, #e0e0e0);border-radius:10px;font-size:0.9rem;color:var(--text, #212121);background:var(--card-bg);">',
            '    </div>',
            '    <div>',
            '      <label for="ech-strava-client-secret" style="display:block;font-size:0.8rem;font-weight:600;margin-bottom:0.25rem;color:var(--text-muted);">Client Secret (write-only)</label>',
            '      <input type="password" id="ech-strava-client-secret" placeholder="Enter new client secret" autocomplete="new-password" style="width:100%;padding:8px 10px;border:1px solid var(--border, #e0e0e0);border-radius:10px;font-size:0.9rem;color:var(--text, #212121);background:var(--card-bg);">',
            '    </div>',
            '    <div>',
            '      <button id="ech-btn-strava-save" type="button" style="padding:8px 20px; font-size:0.85rem; font-weight:700; border:none; border-radius:10px; background:var(--primary, #2196F3); color:#fff; cursor:pointer;">Save Credentials</button>',
            '    </div>',
            '  </div>',
            '  <div id="ech-strava-msg" style="font-size:0.85rem;color:var(--text-muted);margin-top:0.75rem;min-height:1.2em;"></div>',
            '</div>',
            '<div style="background:var(--card-bg); border:1px solid var(--border, #e0e0e0); border-radius:16px; padding:1.25rem;">',
            '  <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:0.75rem;">',
            '    <h3 style="margin:0; font-size:1.15rem; font-weight:700; color:var(--text, #212121);">Strava Integration</h3>',
            '    <span id="ech-strava-badge" style="display:inline-block; padding:2px 10px; font-size:0.75rem; font-weight:700; border-radius:20px; background:var(--border, #e0e0e0); color:var(--text-muted);">Checking...</span>',
            '  </div>',
            '  <p style="margin:0 0 0.75rem 0; font-size:0.85rem; color:var(--text-muted); line-height:1.5;">',
            '    Connect your Strava account to automatically upload completed rides. Create a Strava API application at ',
            '    <a href="https://www.strava.com/settings/api" target="_blank" rel="noopener" style="color:var(--primary, #2196F3);">strava.com/settings/api</a>, ',
            '    then enter the Client ID and Client Secret above.',
            '  </p>',
            '  <div id="ech-strava-details" style="font-size:0.85rem;color:var(--text-muted);margin-bottom:1rem;"></div>',
            '  <div style="display:flex;flex-wrap:wrap;gap:0.5rem;align-items:center;margin-bottom:0.5rem;">',
            '    <button id="ech-btn-strava-auth" type="button" disabled style="padding:8px 16px; font-size:0.85rem; font-weight:700; border:none; border-radius:10px; background:var(--primary, #2196F3); color:#fff; cursor:not-allowed; opacity:0.5;">Authorize with Strava</button>',
            '    <button id="ech-btn-strava-disconnect" type="button" style="padding:8px 16px; font-size:0.85rem; font-weight:600; border:1px solid var(--danger, #c62828); border-radius:10px; background:transparent; color:var(--danger, #c62828); cursor:pointer;">Disconnect Strava</button>',
            '    <button id="ech-btn-strava-refresh" type="button" style="padding:8px 16px; font-size:0.85rem; font-weight:600; border:1px solid var(--border, #e0e0e0); border-radius:10px; background:transparent; color:var(--text, #212121); cursor:pointer;">Refresh Status</button>',
            '  </div>',
            '  <div id="ech-strava-auth-note" style="font-size:0.8rem; color:var(--text-muted); margin-bottom:0.75rem;">Save your API credentials first</div>',
            '  <label style="display:flex;align-items:center;gap:0.5rem;margin-top:0.5rem;font-size:0.85rem;color:var(--text, #212121);">',
            '    <input type="checkbox" id="ech-strava-autoupload" checked>',
            '    <span>Auto-upload completed rides to Strava</span>',
            '  </label>',
            '</div>'
        ].join('\n');

        var host = window.location.hostname || 'echenon.local';
        var domainEl = document.getElementById('ech-callback-domain');
        if (domainEl) domainEl.textContent = host;
        var urlEl = document.getElementById('ech-callback-url');
        if (urlEl) urlEl.textContent = 'http://' + host + '/api/strava/callback';

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
        var authBtn = document.getElementById('ech-btn-strava-auth');
        var authNote = document.getElementById('ech-strava-auth-note');

        if (badge) {
            if (data.authenticated) {
                badge.textContent = 'CONNECTED';
                badge.style.background = 'rgba(46, 125, 50, 0.15)';
                badge.style.color = 'var(--success, #2e7d32)';
            } else if (data.configured) {
                badge.textContent = 'CREDENTIALS SAVED (NOT AUTHORIZED)';
                badge.style.background = 'rgba(249, 168, 37, 0.15)';
                badge.style.color = '#f9a825';
            } else {
                badge.textContent = 'NOT CONFIGURED';
                badge.style.background = 'var(--border, #e0e0e0)';
                badge.style.color = 'var(--text-muted)';
            }
        }

        if (authBtn) {
            if (data.configured) {
                authBtn.disabled = false;
                authBtn.style.opacity = '1';
                authBtn.style.cursor = 'pointer';
            } else {
                authBtn.disabled = true;
                authBtn.style.opacity = '0.5';
                authBtn.style.cursor = 'not-allowed';
            }
        }

        if (authNote) {
            authNote.style.display = data.configured ? 'none' : 'block';
        }

        if (details) {
            var parts = [];
            if (data.athlete_name) parts.push('Athlete: ' + data.athlete_name);
            if (data.client_id) parts.push('Client ID: ' + data.client_id);
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
        var detail = e.detail;
        var targetTab = (detail && detail.tab) ? detail.tab : detail;
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
    window.addEventListener('tab-changed', onTabChanged);

    // Detect strava_error in hash after OAuth redirect (e.g. /#tab-strava?strava_error=...)
    function checkStravaError() {
        var hash = window.location.hash || '';
        var idx = hash.indexOf('strava_error=');
        if (idx !== -1) {
            var errEncoded = hash.substring(idx + 'strava_error='.length);
            var errMsg = decodeURIComponent(errEncoded.replace(/\+/g, ' '));
            if (errMsg) {
                // Ensure the tab is built so setMsg has a target element
                var container = document.getElementById(TAB_ID);
                if (container && !initialized) {
                    buildUI(container);
                    initialized = true;
                }
                setMsg('Strava authorization failed: ' + errMsg);
            }
            // Clean up the hash to remove the error param
            var cleanHash = hash.substring(0, hash.indexOf('?strava_error='));
            if (cleanHash) {
                window.location.hash = cleanHash;
            }
        }
    }
    checkStravaError();

    var el = document.getElementById(TAB_ID);
    if (el && el.classList.contains('active')) {
        buildUI(el);
        initialized = true;
        fetchStravaStatus();
    }
})();

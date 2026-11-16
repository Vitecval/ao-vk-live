<?php
/**
 * Plugin Name: Rubi-Ka Horizon Reports
 * Description: Accept reviewed anonymous Horizon diagnostics and email them privately. No public log files are created.
 * Version: 1.0.1
 * Requires PHP: 7.4
 * License: MIT
 */
if (!defined('ABSPATH')) { exit; }

function rkh_report_options() {
    $o = get_option('rkh_report_options', array());
    return array('enabled' => !empty($o['enabled']), 'email' => isset($o['email']) && is_email($o['email']) ? $o['email'] : 'dev@asyncstuff.com');
}
function rkh_report_sanitize_options($value) {
    if (!is_array($value)) { $value = array(); }
    $email = isset($value['email']) ? sanitize_email($value['email']) : '';
    if (!is_email($email)) { $email = 'dev@asyncstuff.com'; }
    return array('enabled' => !empty($value['enabled']), 'email' => $email);
}
add_action('admin_init', function () {
    register_setting('rkh_reports', 'rkh_report_options', array('type' => 'array', 'sanitize_callback' => 'rkh_report_sanitize_options', 'show_in_rest' => false));
});
add_action('admin_menu', function () {
    add_options_page('Horizon Reports', 'Horizon Reports', 'manage_options', 'rkh-reports', 'rkh_report_settings');
});
function rkh_report_settings() {
    if (!current_user_can('manage_options')) { return; }
    $o = rkh_report_options();
    echo '<div class="wrap"><h1>Horizon Reports</h1><p>Reviewed diagnostics are sent by WordPress email. No reports are published or saved in the media library.</p><form action="options.php" method="post">';
    settings_fields('rkh_reports');
    echo '<table class="form-table"><tr><th scope="row">Receive reports</th><td><label><input type="checkbox" name="rkh_report_options[enabled]" value="1" ' . checked($o['enabled'], true, false) . '> Enable anonymous Horizon reports</label></td></tr>';
    echo '<tr><th scope="row"><label for="rkh-email">Send reports to</label></th><td><input id="rkh-email" name="rkh_report_options[email]" type="email" class="regular-text" required value="' . esc_attr($o['email']) . '"><p class="description">Use a working WordPress mail/SMTP setup. WordPress accepting a message does not confirm inbox delivery.</p></td></tr></table>';
    submit_button();
    echo '</form><p>Limit: 1 MiB per request, five attempts per network address every 15 minutes, and 60 attempts per hour overall. Only short-lived report IDs and keyed address hashes are used for duplicate/spam control. Log contents are never stored in options or uploads.</p></div>';
}
function rkh_report_response($data, $status = 200) {
    $r = new WP_REST_Response($data, $status);
    $r->header('Cache-Control', 'no-store, private');
    return $r;
}
add_action('rest_api_init', function () {
    register_rest_route('rubika-horizon/v1', '/reports/status', array('methods' => 'GET', 'permission_callback' => '__return_true', 'callback' => function () {
        $o = rkh_report_options();
        return rkh_report_response(array('enabled' => $o['enabled'], 'schema' => 1, 'encoding' => 'gzip-base64', 'maxBytes' => 1048576, 'maxDecodedBytes' => 4194304));
    }));
    register_rest_route('rubika-horizon/v1', '/reports', array('methods' => 'POST', 'permission_callback' => '__return_true', 'callback' => 'rkh_receive_report'));
});
// An exclusive database option serializes quota checks and mail submission.
// Compare-and-delete prevents an expired sender from deleting a newer lease.
function rkh_report_unlock($value) {
    global $wpdb;
    $wpdb->query($wpdb->prepare("DELETE FROM {$wpdb->options} WHERE option_name = %s AND option_value = %s", 'rkh_report_send_lock', maybe_serialize($value)));
    wp_cache_delete('rkh_report_send_lock', 'options');
}
function rkh_report_lock() {
    $old = get_option('rkh_report_send_lock', false);
    if (is_array($old) && isset($old['until']) && $old['until'] < time()) { rkh_report_unlock($old); }
    $value = array('token' => wp_generate_uuid4(), 'until' => time() + 120);
    return add_option('rkh_report_send_lock', $value, '', false) ? $value : false;
}
function rkh_receive_report($request) {
    $o = rkh_report_options();
    if (!$o['enabled']) { return new WP_Error('rkh_disabled', 'Reporting is not enabled.', array('status' => 503)); }
    $body = $request->get_body();
    if (strlen($body) > 1048576) { return new WP_Error('rkh_size', 'Report exceeds 1 MiB.', array('status' => 413)); }
    if (stripos($request->get_header('content-type'), 'application/json') !== 0) { return new WP_Error('rkh_type', 'Use JSON.', array('status' => 415)); }
    $p = $request->get_json_params();
    if (!is_array($p) || !isset($p['schema'], $p['id'], $p['version'], $p['title'], $p['report'], $p['encoding'], $p['bytes']) || $p['schema'] !== 1 ||
        !is_string($p['id']) || !preg_match('/^[a-f0-9]{32}$/D', $p['id']) ||
        !is_string($p['version']) || !preg_match('/^[0-9]{1,4}\.[0-9]{1,4}\.[0-9]{1,4}(?:-preview\.[0-9]{1,6})?$/D', $p['version']) ||
        !is_string($p['title']) || strlen($p['title']) > 512 || !is_string($p['report']) || strlen($p['report']) < 1 || strlen($p['report']) > 1000000 || $p['encoding'] !== 'gzip-base64' || !is_int($p['bytes']) || $p['bytes'] < 1 || $p['bytes'] > 4194304) {
        return new WP_Error('rkh_invalid', 'Invalid report.', array('status' => 400));
    }
    if (!function_exists('gzdecode')) { return new WP_Error('rkh_zlib', 'Report compression support is unavailable.', array('status' => 503)); }
    $done = 'rkh_report_sent_' . $p['id'];
    if (get_transient($done)) { return rkh_report_response(array('accepted' => true, 'id' => $p['id'], 'duplicate' => true)); }
    $lock = rkh_report_lock();
    if (!$lock) { return new WP_Error('rkh_busy', 'Reporting is busy. Try later.', array('status' => 429)); }
    try {
        if (get_transient($done)) { return rkh_report_response(array('accepted' => true, 'id' => $p['id'], 'duplicate' => true)); }
        $global_key = 'rkh_report_hour_' . gmdate('YmdH');
        $global_count = (int) get_transient($global_key);
        $ip = isset($_SERVER['REMOTE_ADDR']) ? (string) $_SERVER['REMOTE_ADDR'] : 'unknown';
        // Forwarded headers are intentionally not trusted. A reverse proxy's
        // real-client-address support should be configured at the web server.
        $ip_key = 'rkh_report_ip_' . substr(hash_hmac('sha256', $ip, wp_salt('auth')), 0, 32);
        $ip_count = (int) get_transient($ip_key);
        if ($global_count >= 60 || $ip_count >= 5) { return new WP_Error('rkh_limit', 'Report limit reached. Try later.', array('status' => 429)); }
        set_transient($global_key, $global_count + 1, 3600);
        set_transient($ip_key, $ip_count + 1, 900);
        // Charge the quota before decompression. Malformed compressed reports
        // cannot bypass rate limits to repeatedly consume expansion work.
        $compressed = base64_decode($p['report'], true);
        if ($compressed === false) { return new WP_Error('rkh_encoding', 'Invalid report encoding.', array('status' => 400)); }
        $decoded = @gzdecode($compressed, 4194304);
        if ($decoded === false || strlen($decoded) !== $p['bytes'] || !preg_match('//u', $decoded)) { return new WP_Error('rkh_encoding', 'Invalid compressed report.', array('status' => 400)); }
        $title = substr(str_replace(array("\r", "\n"), ' ', sanitize_text_field($p['title'])), 0, 120);
        $report = preg_replace('/[\x00-\x08\x0B\x0C\x0E-\x1F\x7F]/', '', $decoded);
        $message = 'Anonymous Rubi-Ka Horizon report ' . $p['id'] . "\nApp version: " . $p['version'] . "\n\n" . $report;
        // No client-supplied recipient, mail headers, reply address or file path.
        $accepted = wp_mail($o['email'], '[Horizon] ' . $title, $message, array('Content-Type: text/plain; charset=UTF-8'));
        if (!$accepted) { return new WP_Error('rkh_mail', 'Mail service did not accept the report. Try later.', array('status' => 503)); }
        set_transient($done, true, 86400);
        return rkh_report_response(array('accepted' => true, 'id' => $p['id']));
    } finally { rkh_report_unlock($lock); }
}

Rubi-Ka Horizon Reports 1.0.1 — WordPress plugin

1. In WordPress, Plugins > Add New > Upload Plugin, select the supplied ZIP.
2. Install and activate it.
3. Open Settings > Horizon Reports. Confirm dev@asyncstuff.com (or your chosen
   recipient), tick Enable anonymous Horizon reports, and save.
4. Use an existing working WordPress mail/SMTP plugin. Test inbox delivery with
   a small test report you submit yourself from Horizon. No email was sent by
   the automated local tests.

The Horizon app submits to:
https://asyncstuff.com/wp-json/rubika-horizon/v1/reports

Status (no report is submitted):
https://asyncstuff.com/wp-json/rubika-horizon/v1/reports/status

Reporting is disabled by default. The endpoint is anonymous and uses fixed
server-side email settings. There is no SMTP password or GitHub token in Horizon.
The player reviews the exact excerpts before Send report. Full local ZIPs remain
on the player's PC; the service receives plain diagnostic text and never creates
a public file or stores the report body in WordPress options/uploads.

Limits: compressed JSON request up to 1 MiB; decoded diagnostic text up to 4 MiB; five attempts per address every 15 minutes; 60 per
hour overall. Keyed address hashes expire after 15 minutes; duplicate report IDs
expire after one day. Mail is serialized with a short exclusive lease. Hosting
access logs and your email retention are controlled by your hosting/mail setup.
Configure a 1 MiB request-body limit at your web server and normal spam/WAF rules
if needed. If behind a proxy, configure its trusted real-client-IP handling at
the server; this plugin does not trust arbitrary forwarded headers.

A successful response means WordPress accepted the email for sending. Delivery
still depends on the site's mail service. Requests that time out can be retried
with the same report ID; accepted reports are deduplicated for one day.

Version 1.0.1 supports compressed full-log reports. If 1.0.0 is installed, upload
this ZIP and choose Replace current with uploaded. The app checks this capability
before submitting any diagnostic text.

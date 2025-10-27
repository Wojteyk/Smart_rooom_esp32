// Simple DNS server to redirect all queries to the AP IP for captive portal
#pragma once

/**
 * Start the DNS server task. It will respond to any DNS query with the
 * configured captive portal IP (192.168.4.1 by default).
 */
void dns_server_start(void);

/**
 * Stop the DNS server task.
 */
void dns_server_stop(void);

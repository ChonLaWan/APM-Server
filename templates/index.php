<?php
// APM default page. Shows what is running; phpinfo() with ?info=1 (this machine only - APM may be on the LAN)
$isLocal = in_array($_SERVER['REMOTE_ADDR'] ?? '', ['127.0.0.1', '::1'], true);
if (isset($_GET['info']) && $isLocal) { phpinfo(); exit; }
$db = 'n/a';
try {
    $pdo = new PDO('mysql:host=127.0.0.1;port=' . (ini_get('mysqli.default_port') ?: 3306), 'root', '');
    $db = $pdo->query('select version()')->fetchColumn();
} catch (Throwable $e) {
    $db = 'not reachable (' . htmlspecialchars($e->getMessage()) . ')';
}
$jit = function_exists('opcache_get_status') ? (opcache_get_status(false)['jit']['on'] ?? false) : false;
?><!doctype html>
<html lang="en"><head><meta charset="utf-8"><title>APM</title>
<style>body{font:15px/1.5 system-ui,sans-serif;margin:2rem;color:#222}td{padding:.2rem 1rem .2rem 0}code{background:#f3f3f3;padding:0 .3rem}</style>
</head><body>
<h1>APM</h1>
<table>
<tr><td>Server</td><td><code><?= htmlspecialchars($_SERVER['SERVER_SOFTWARE'] ?? '') ?></code></td></tr>
<tr><td>Protocol</td><td><code><?= htmlspecialchars($_SERVER['SERVER_PROTOCOL'] ?? '') ?></code> <?= !empty($_SERVER['HTTPS']) ? 'TLS' : '' ?></td></tr>
<tr><td>PHP</td><td><code><?= PHP_VERSION ?></code> <?= PHP_SAPI ?>, OPcache JIT <?= $jit ? 'on' : 'off' ?></td></tr>
<tr><td>MariaDB</td><td><code><?= $db ?></code></td></tr>
<tr><td>Extensions</td><td><?= implode(' ', array_map('strtolower', get_loaded_extensions())) ?></td></tr>
</table>
<?php if ($isLocal): ?><p><a href="?info=1">phpinfo()</a></p><?php endif; ?>
</body></html>

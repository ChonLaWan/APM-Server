<?php
// mkcert.php - local CA + server certificate for APM, using PHP's openssl extension
// (APM ships no openssl.exe). Run with APM's own php.exe:
//   php.exe -n -d extension_dir=<php>\ext -d extension=openssl mkcert.php <ssl-dir> [hostname|ip ...]
// Creates once: apm-ca.key/.crt (10 years). Issues: <first-host>.key/.crt (825 days, SAN = all hosts).

$dir   = $argv[1] ?? null;
$hosts = array_slice($argv, 2) ?: ['localhost'];
if (!$dir) { fwrite(STDERR, "usage: mkcert.php <ssl-dir> [host ...]\n"); exit(2); }
if (!extension_loaded('openssl')) {
    fwrite(STDERR, "[ERR] PHP openssl extension not loaded (check -d extension_dir=\"...\" - quote it, folder names may contain ( ) or spaces)\n");
    exit(1);
}
if (!is_dir($dir)) mkdir($dir, 0700, true);

function fail(string $what): never {
    fwrite(STDERR, "[ERR] $what\n");
    while ($e = openssl_error_string()) fwrite(STDERR, "  $e\n");
    exit(1);
}

// self-contained OpenSSL config: no dependency on an external openssl.cnf
$san = implode(',', array_map(fn($h) => filter_var($h, FILTER_VALIDATE_IP) ? "IP:$h" : "DNS:$h", $hosts));
$cnf = "$dir/.apm-openssl.cnf";
file_put_contents($cnf, <<<CNF
[req]
distinguished_name = dn
[dn]
[apm_ca]
basicConstraints = critical,CA:TRUE,pathlen:0
keyUsage = critical,keyCertSign,cRLSign
subjectKeyIdentifier = hash
[apm_server]
basicConstraints = CA:FALSE
keyUsage = critical,digitalSignature,keyEncipherment
extendedKeyUsage = serverAuth
subjectAltName = $san
CNF);
$opt = ['config' => $cnf, 'digest_alg' => 'sha256'];
$ec  = $opt + ['private_key_type' => OPENSSL_KEYTYPE_EC, 'curve_name' => 'prime256v1'];

$caKey = "$dir/apm-ca.key";
$caCrt = "$dir/apm-ca.crt";
if (!is_file($caKey) || !is_file($caCrt)) {
    $key = openssl_pkey_new($ec) ?: fail('CA key');
    $csr = openssl_csr_new(['commonName' => 'APM Local Dev CA', 'organizationName' => 'APM'], $key, $opt) ?: fail('CA csr');
    $crt = openssl_csr_sign($csr, null, $key, 3650, $opt + ['x509_extensions' => 'apm_ca'], random_int(1, PHP_INT_MAX)) ?: fail('CA sign');
    openssl_pkey_export_to_file($key, $caKey, null, $opt) ?: fail('CA key write');
    openssl_x509_export_to_file($crt, $caCrt) ?: fail('CA crt write');
    echo "[OK] new CA: $caCrt\n";
}

$caPriv = openssl_pkey_get_private(file_get_contents($caKey)) ?: fail('CA key read');
$key = openssl_pkey_new($ec) ?: fail('server key');
$csr = openssl_csr_new(['commonName' => $hosts[0]], $key, $opt) ?: fail('server csr');
$crt = openssl_csr_sign($csr, file_get_contents($caCrt), $caPriv, 825, $opt + ['x509_extensions' => 'apm_server'], random_int(1, PHP_INT_MAX)) ?: fail('server sign');
$name = preg_replace('/[^a-z0-9.-]/i', '_', $hosts[0]);
openssl_pkey_export_to_file($key, "$dir/$name.key", null, $opt) ?: fail('server key write');
openssl_x509_export_to_file($crt, "$dir/$name.crt") ?: fail('server crt write');
unlink($cnf);
echo "[OK] cert $dir/$name.crt  SAN=$san\n";

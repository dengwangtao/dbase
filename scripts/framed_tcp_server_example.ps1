$client = New-Object System.Net.Sockets.TcpClient
$client.Connect("127.0.0.1", 9860)
$stream = $client.GetStream()

$payload = [System.Text.Encoding]::UTF8.GetBytes("hello")
$len = [System.BitConverter]::GetBytes([System.Net.IPAddress]::HostToNetworkOrder($payload.Length))
$frame = New-Object byte[] (4 + $payload.Length)
[Array]::Copy($len, 0, $frame, 0, 4)
[Array]::Copy($payload, 0, $frame, 4, $payload.Length)

$stream.Write($frame, 0, $frame.Length)

$hdr = New-Object byte[] 4
$stream.Read($hdr, 0, 4) | Out-Null
$respLen = [System.Net.IPAddress]::NetworkToHostOrder([System.BitConverter]::ToInt32($hdr, 0))
$resp = New-Object byte[] $respLen
$stream.Read($resp, 0, $respLen) | Out-Null
[System.Text.Encoding]::UTF8.GetString($resp)

$client.Close()
# Parallel Stockfish relabeling. Runs N shards concurrently (each its own Stockfish), then
# concatenates them. Usage: .\tools\relabel_parallel.ps1 -procs 6 -depth 10
param(
    [int]$procs = 6,
    [int]$depth = 10,
    [string]$infile = "data\selfplay_gen1.csv",
    [string]$out = "data\distill_gen1.csv"
)

$root = "C:\PYTHON\chess"
$py = "$root\.venv\Scripts\python.exe"
$inFull = Join-Path $root $infile
$outFull = Join-Path $root $out

$running = @()
for ($i = 0; $i -lt $procs; $i++) {
    $of = "$root\data\rl_$i.csv"
    $p = Start-Process -FilePath $py `
        -ArgumentList @("$root\tools\relabel_stockfish.py", "--infile", $inFull, "--outfile", $of,
                        "--depth", "$depth", "--shard", "$i", "--nshards", "$procs") `
        -PassThru -WindowStyle Hidden
    $running += $p
}
Write-Host "launched $procs relabel shards (Stockfish depth $depth)..."
$running | ForEach-Object { $_.WaitForExit() }

$parts = (0..($procs - 1) | ForEach-Object { "$root\data\rl_$_.csv" }) -join "+"
cmd /c "copy /b $parts `"$outFull`"" | Out-Null
$lines = (Get-Content $outFull | Measure-Object -Line).Lines
Write-Host "combined -> $outFull ($lines positions)"
0..($procs - 1) | ForEach-Object { Remove-Item "$root\data\rl_$_.csv" -ErrorAction SilentlyContinue }

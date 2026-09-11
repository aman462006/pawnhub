# Parallel self-play data generation. Launches N engine datagen processes (each on its own
# seed -> its own file), waits for all, then concatenates them into one dataset.
#
# Usage: .\tools\gen_selfplay.ps1 -procs 6 -games 4200 -nodes 5000 -out data\selfplay_gen1.csv
param(
    [int]$procs = 6,
    [long]$games = 4200,
    [long]$nodes = 5000,
    [string]$out = "data\selfplay_gen1.csv",
    [int]$seedbase = 1000    # different seedbase => different games (avoid duplicate self-play)
)

$root = "C:\PYTHON\chess"
$exe = "$root\engine\build\engine.exe"
$outFull = Join-Path $root $out

$running = @()
for ($i = 0; $i -lt $procs; $i++) {
    $seed = $seedbase + $i
    $of = "$root\data\sp_$i.csv"
    $p = Start-Process -FilePath $exe -ArgumentList @("datagen", "$games", "$nodes", "$of", "$seed") `
                       -PassThru -WindowStyle Hidden
    $running += $p
}
Write-Host "launched $procs datagen processes ($games games x $nodes nodes each)..."
$running | ForEach-Object { $_.WaitForExit() }

# Concatenate all part files into one dataset.
$parts = (0..($procs - 1) | ForEach-Object { "$root\data\sp_$_.csv" }) -join "+"
cmd /c "copy /b $parts `"$outFull`"" | Out-Null
$lines = (Get-Content $outFull | Measure-Object -Line).Lines
Write-Host "combined -> $outFull ($lines positions)"
0..($procs - 1) | ForEach-Object { Remove-Item "$root\data\sp_$_.csv" -ErrorAction SilentlyContinue }

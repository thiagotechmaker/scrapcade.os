# ScrapCade OS: publica a versao compilada nas GitHub Releases (precisa do gh logado: gh auth login).
# Uso (no PowerShell do Windows):  powershell -File scripts\publicar-versao.ps1 [-So64]
# Sobe para a tag v<VERSION> (SemVer, do arquivo VERSION na raiz), como versao normal (Latest: o atualizador so le essa):
#   - scrapcade-os-v<versao>-pc-64bits.iso       para instalar do zero (Rufus em qualquer modo, balenaEtcher, DVD)
#   - tos.squashfs, bzImage, initrd.gz           usados pela atualizacao automatica (Sistema > Atualizar o ScrapCade OS)
#   - BUILD_ID                                   numero da compilacao: o atualizador compara com o do PC
#   - SHA256SUMS                                 confere todos
#   - os mesmos de 32 bits (-32 no nome), se a compilacao de 32 bits for a mesma e passou no teste dela
# Travas: so publica o que passou nos testes automaticos, com a versao no CHANGELOG e sem tag repetida.
param([string]$Repo = "thiagotechmaker/scrapcade.os", [switch]$So64,
      [string]$Pasta = (Join-Path (Split-Path $PSScriptRoot) "output\x86_64\images\atualizacao"))
$ErrorActionPreference = "Stop"
$raiz = Split-Path $PSScriptRoot
function Para($m) { Write-Host "PARADO: $m"; exit 1 }

$b = (Get-Content "$Pasta\BUILD_ID").Trim()
$v = (Get-Content "$Pasta\VERSION").Trim()
if ($v -ne (Get-Content (Join-Path $raiz "VERSION")).Trim()) { Para "a compilacao e da versao $v, mas o arquivo VERSION diz outra: compile de novo" }
$ap = Join-Path $raiz "teste/aprovado"
if (-not (Test-Path $ap) -or (Get-Content $ap).Trim() -ne $b) { Para "a compilacao $b nao passou nos testes automaticos. Rode: bash tests/testar.sh" }

# notas da versao: a secao dela no CHANGELOG
$cl = Get-Content (Join-Path $raiz "CHANGELOG.md") -Encoding UTF8
$ini = ($cl | Select-String -Pattern ("^## \[" + [regex]::Escape($v) + "\]")).LineNumber
if (-not $ini) { Para "falta a secao [$v] no CHANGELOG.md" }
$fim = ($cl | Select-Object -Skip $ini | Select-String -Pattern "^## \[" | Select-Object -First 1).LineNumber
$secao = if ($fim) { $cl[$ini..($ini + $fim - 2)] } else { $cl[$ini..($cl.Count - 1)] }

gh release view "v$v" -R $Repo *> $null
if ($LASTEXITCODE -eq 0) { Para "a tag v$v ja existe no GitHub: suba o numero no VERSION" }

$iso64 = "scrapcade-os-v$v-pc-64bits.iso"
$arq = "tos.squashfs","bzImage","initrd.gz","BUILD_ID","SHA256SUMS",$iso64 | ForEach-Object { Join-Path $Pasta $_ }
$notas = "**ScrapCade OS v$v** para PC de 64 bits (BIOS antigo e UEFI).`n`n" +
  "**Instalar do zero:** baixe ``$iso64`` e grave num pen drive de 4 GB ou mais com o Rufus (modo ISO ou DD, tanto faz) ou o balenaEtcher. " +
  "No primeiro boot o ScrapCade arruma o pen drive sozinho (sistema + partição SCRAP-DADOS para os jogos). Para ligar sem o pen drive: *Sistema > Instalar no disco do computador*.`n`n" +
  "**Já tem o ScrapCade:** ele avisa sozinho e atualiza em *Sistema > Atualizar o ScrapCade OS*, sem perder jogos nem saves. Se a versão nova não ligar, volta sozinho para a anterior. " +
  "Quem está na v0.19.0-beta ou antes precisa gravar o ``.iso`` uma vez (o atualizador antigo só reconhece tags de data)."

# 32 bits: vai junto quando e a mesma compilacao (bash build-i686.sh <BUILD_ID>); sem a aprovacao dela, para (ou -So64)
$P32 = $Pasta -replace "x86_64", "i686"
if ((Test-Path "$P32\BUILD_ID") -and (Get-Content "$P32\BUILD_ID").Trim() -eq $b) {
  $ap32 = Join-Path $raiz "teste/aprovado-32"
  if ((Test-Path $ap32) -and (Get-Content $ap32).Trim() -eq $b) {
    $iso32 = "scrapcade-os-v$v-pc-32bits.iso"
    $arq += "tos-32.squashfs","bzImage-32","initrd-32.gz","SHA256SUMS-32",$iso32 | ForEach-Object { Join-Path $P32 $_ }
    $notas += "`n`n**PC de 32 bits** (Pentium 4, Pentium M, Atom): ``$iso32``. Leva os emuladores leves (sem PS2, GameCube, PSP, Saturn e Dreamcast)."
  } elseif (-not $So64) { Para "a compilacao de 32 bits $b nao passou no teste dela (bash tests/testar-32.sh). Para subir so a de 64 bits: -So64" }
}
$notas += "`n`nJogos, BIOS e ROMs não acompanham o sistema.`n`n### O que mudou`n" + ($secao -join "`n").Trim()
foreach ($f in $arq) { if (-not (Test-Path $f)) { Para "falta o arquivo $f" } }

$commit = (git -C $raiz -c safe.directory=* rev-parse HEAD).Trim()
$tmp = New-TemporaryFile
[IO.File]::WriteAllText($tmp, $notas, (New-Object Text.UTF8Encoding $false))   # UTF-8 sem BOM
gh release create "v$v" -R $Repo --target $commit --title "ScrapCade OS v$v" --notes-file $tmp --latest @arq
Remove-Item $tmp

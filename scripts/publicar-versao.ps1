# ScrapCade: publica a versao compilada nas GitHub Releases (precisa do gh logado: gh auth login).
# Uso (no PowerShell do Windows):  powershell -File scripts\publicar-versao.ps1
# Sobe para a tag v<BUILD_ID>:
#   - scrapcade-os-<versao>-pc-64bits.iso         para instalar do zero (Rufus em qualquer modo, balenaEtcher, DVD)
#   - tos.squashfs, bzImage, initrd.gz        usados pela atualizacao automatica (Sistema > Atualizar o ScrapCade OS)
#   - SHA256SUMS                              confere todos
param([string]$Repo = "thiagotechmaker/scrapcade.os", [string]$Pasta = "\\wsl.localhost\Ubuntu-24.04\home\thiago\t-os\output\x86_64\images\atualizacao")
$ErrorActionPreference = "Stop"
$v = (Get-Content "$Pasta\BUILD_ID").Trim()
# trava: so publica a compilacao aprovada pelos testes automaticos (bash tests/testar.sh)
$ap = Join-Path (Split-Path $PSScriptRoot) "teste/aprovado"
if (-not (Test-Path $ap) -or (Get-Content $ap).Trim() -ne $v) {
  Write-Host "PARADO: a compilacao $v nao passou nos testes automaticos. Rode: bash tests/testar.sh"
  exit 1
}
$data = "{0}/{1}/{2} {3}:{4}" -f $v.Substring(6,2), $v.Substring(4,2), $v.Substring(0,4), $v.Substring(8,2), $v.Substring(10,2)
$commit = (git -C (Split-Path $PSScriptRoot) -c safe.directory=* rev-parse HEAD).Trim()
$notas = @"
**ScrapCade OS $data** para PC de 64 bits (BIOS antigo e UEFI).

**Instalar do zero:** baixe ``scrapcade-os-$v-pc-64bits.iso`` e grave num pen drive de 4 GB ou mais com o Rufus (modo ISO ou DD, tanto faz) ou o balenaEtcher. No primeiro boot o ScrapCade arruma o pen drive sozinho (sistema + particao SCRAP-DADOS para os jogos). Para ligar sem o pen drive: *Sistema > Instalar no disco do computador*.

**Ja tem o ScrapCade:** ele avisa sozinho e atualiza em *Sistema > Atualizar o ScrapCade OS*, sem perder jogos nem saves. Se a versao nova nao ligar, volta sozinho para a anterior.

Jogos, BIOS e ROMs nao acompanham o sistema.
"@
$arq = "tos.squashfs","bzImage","initrd.gz","SHA256SUMS","scrapcade-os-$v-pc-64bits.iso" | ForEach-Object { Join-Path $Pasta $_ }
# 32 bits: sobe junto se a compilacao for da mesma versao (bash build-i686.sh <BUILD_ID>) e o teste dela passou
$P32 = $Pasta -replace "x86_64", "i686"
if ((Test-Path "$P32\BUILD_ID") -and (Get-Content "$P32\BUILD_ID").Trim() -eq $v) {
  $ap32 = Join-Path (Split-Path $PSScriptRoot) "teste/aprovado-32"
  if ((Test-Path $ap32) -and (Get-Content $ap32).Trim() -eq $v) {
    $arq += "tos-32.squashfs","bzImage-32","initrd-32.gz","SHA256SUMS-32","scrapcade-os-$v-pc-32bits.iso" | ForEach-Object { Join-Path $P32 $_ }
    $notas += "`n`n**PC de 32 bits** (Pentium 4, Pentium M, Atom): ``scrapcade-os-$v-pc-32bits.iso``. Leva os emuladores leves (sem PS2, GameCube, PSP, Saturn e Dreamcast)."
  } else { Write-Host "32 bits: a compilacao $v nao passou no teste de 32 bits; sobe so a de 64 bits" }
}
gh release create "v$v" -R $Repo --target $commit --title "ScrapCade OS $data" --notes $notas --latest @arq

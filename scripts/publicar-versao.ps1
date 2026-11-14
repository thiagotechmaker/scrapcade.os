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
gh release create "v$v" -R $Repo --target $commit --title "ScrapCade OS $data" --notes $notas --latest @arq

# Do que o T-OS é feito

Visão geral de cada peça do T-OS, com versão e função, e pontos para pesquisar melhorias.
Estado de novembro de 2026.

O sistema é montado em camadas: construção → boot → kernel → sistema base → gráficos e som → ScrapCade → emuladores → desktop.

---

## 1. Ferramentas de construção (ficam no PC, não vão para o sistema)

| Peça | Versão | Para que serve |
|---|---|---|
| **Buildroot** | 2026.02.3 (LTS) | Baixa o código-fonte e compila tudo |
| **Compilador Bootlin** | GCC 14.3.0, glibc | Compila os programas para x86_64 |
| **WSL2 + Ubuntu** | 24.04 | Linux dentro do Windows onde a compilação roda |
| **genimage** | — | Monta o `disk.img` com as partições |
| **xorriso** | 1.5.4 | Monta o `.iso` |
| **QEMU + OVMF/SeaBIOS** | — | PC virtual para testar UEFI e BIOS antigo |

Arquivos principais do repositório:

- `external/configs/tos_x86_64_defconfig`: o que entra no sistema.
- `external/board/x86_64/`: boot, kernel e montagem das imagens.
- `external/board/common/rootfs-overlay/`: scripts próprios do T-OS.
- `external/package/`: ScrapCade, RetroArch e os núcleos.

**Para pesquisar:**

- Compilação no GitHub Actions, gerando as imagens a cada versão.
- Guardar o cache do ccache entre compilações.
- Builds reproduzíveis (`BR2_REPRODUCIBLE`).

---

## 2. Boot

| Peça | Versão | Função |
|---|---|---|
| **syslinux / isolinux** | 6.03 | Menu com fundo no **BIOS antigo** (`vesamenu`, `mbr.bin`, `isohdpfx`) |
| **GRUB** | 2.12 | Menu no **UEFI** (`bootx64.efi`); também liga o disco instalado no HD |
| **memtest86+** | — | Teste de memória (só BIOS) |
| **Fundo do menu** | próprio, 640×480 | `boot-fundo.png` |

Opções do menu:

- Iniciar o T-OS.
- **Modo de compatibilidade** (`tos.seguro=1`: desenho em software).
- Iniciar mostrando as mensagens.
- Teste de memória.
- Ligar pelo disco do computador.

**Para pesquisar:**

- **Secure Boot** (shim assinado), para ligar sem desligar a proteção.
- Splash animado durante o carregamento (Plymouth ou psplash).
- Tema gráfico no GRUB (precisa de fonte `.pf2`).
- Lembrar a última opção escolhida.

---

## 3. Kernel e arranque

| Peça | Versão / ajuste | Função |
|---|---|---|
| **Kernel Linux** | 6.12.47 LTS | Conversa com o hardware |
| **Ajustes do T-OS** | `PREEMPT`, 1000 Hz | Menos atraso no controle e no áudio |
| **Drivers embutidos** | USB 1.1/2.0/3.0, SATA (AHCI), NVMe, cartão SD, exFAT, FAT, squashfs (zstd), OverlayFS, ISO9660 | Achar o pen drive e o HD em qualquer máquina |
| **Drivers em módulo** | Vídeo: i915 (Intel), amdgpu e radeon (AMD), nouveau (NVIDIA). Som: codecs HDA. Rede: cabo, Wi-Fi e USB | Só o da máquina é carregado |
| **initrd próprio** | BusyBox + glibc, 1,4 MB | Acha o `tos.squashfs` e monta o sistema |
| **tos.squashfs** | zstd, ~250 MB | O sistema todo, somente leitura, com uma camada na memória |

Arquivos: `external/board/x86_64/initramfs/init` (initrd) e `external/board/x86_64/linux-tos.fragment` (drivers do kernel).

**Para pesquisar:**

- **Patches de desempenho:** BORE/EEVDF ajustado, CachyOS, linux-zen.
- **zram:** memória comprimida, que ajuda máquinas com 2 GB.
- **Governador de CPU:** `schedutil` ou `performance` no modo jogo.
- **Tempo de boot:** medir com `bootchart` e tirar módulos que nunca carregam.
- **Kernel 32 bits (i686)** para PCs bem antigos.

---

## 4. Discos e partições

| Partição | Formato | O que tem |
|---|---|---|
| **TOS-BOOT** | FAT32, 1 GB, tabela MBR | Menu de boot (syslinux e GRUB), kernel, initrd e `boot/tos.squashfs` |
| **TOS-DADOS** | exFAT, o resto do disco | `roms`, `bios`, `saves`, `themes`, `Jogos novos` e `system` (senha, Wi-Fi, configurações, registros) |

- A TOS-DADOS é criada no primeiro boot, no espaço livre, sem redimensionar nada.
- Gravado pelo **modo ISO do Rufus** (sem espaço para partição), os dados ficam na pasta `tos-dados` do próprio pen drive.
- Atualizar o T-OS é trocar só o `tos.squashfs`; a TOS-DADOS nunca é mexida.

---

## 5. Sistema base (serviços)

| Peça | Função |
|---|---|
| **BusyBox** | `init` (primeiro processo), terminal e comandos básicos |
| **eudev** | Detecta o hardware e carrega os drivers |
| **D-Bus** | Comunicação entre serviços |
| **ConnMan + wpa_supplicant** | Rede com cabo e Wi-Fi |
| **linux-firmware** | Firmwares de Wi-Fi, rede e vídeo (redistribuíveis) |
| **util-linux** | `sfdisk`, `blkid`, `partx` (partições) |
| **exfatprogs, dosfstools** | Formatar TOS-DADOS (exFAT) e TOS-BOOT (FAT32) |
| **iptables** | Firewall |
| **efibootmgr** | Registra o T-OS no menu de boot da placa-mãe (instalador) |

Scripts próprios em `/etc/init.d`:

| Script | Função |
|---|---|
| `S00dados` | Acha ou cria a TOS-DADOS; liga a pasta do Wi-Fi a ela |
| `S01primeiro-boot` | Pede a senha do administrador e guarda na TOS-DADOS |
| `S98diagnostico` | Gera `Registros/diagnostico.txt` com placas, drivers, rede e controles |
| `S99scrapcade` | Abre o menu de jogos; ajusta controles, volume e modo de compatibilidade |

Scripts em `/usr/bin`:

| Script | Função |
|---|---|
| `tos-jogar` | Abre o emulador certo com a configuração central |
| `tos-instalar` | Instala no HD |
| `tos-desktop` | Abre o modo desktop |
| `tos-navegador` | Abre o navegador |

**Para pesquisar:**

- **Bluetooth (BlueZ):** ainda não tem. É o maior buraco para controles sem fio de PS4/PS5/Switch.
- **Samba:** pasta compartilhada na rede, para ver os jogos no Windows sem a página (planejado).
- **SSH:** dropbear, leve, desligado por padrão.
- **Fuso horário e relógio pela internet (NTP):** o relógio ainda mostra UTC.
- **Atualização automática:** baixar o `tos.squashfs` novo das GitHub Releases.

---

## 6. Gráficos e som

| Peça | Versão | Função |
|---|---|---|
| **Mesa** | 26.0.1 | OpenGL / OpenGL ES. Drivers: iris, crocus e i915 (Intel); radeonsi e r600 (AMD); nouveau (NVIDIA); llvmpipe (software) |
| **LLVM** | 21.1.8 | Usado pelo llvmpipe e pelos drivers AMD |
| **libdrm + GBM + EGL** | — | Desenho direto na tela (KMS), sem X |
| **SDL2** | 2.32.10 | Janela, controle, teclado e som do ScrapCade |
| **SDL2_image / SDL2_ttf** | 2.8.12 / 2.24.0 | Capas (PNG/JPG) e textos |
| **SDL_GameControllerDB** | 2.295 controles | Banco da comunidade para reconhecer controles |
| **ALSA** | 1.2.15.3 | Som |

**Para pesquisar:**

- **Vulkan:** drivers ANV (Intel) e RADV (AMD) da Mesa. Deixa Dolphin, PCSX2 e N64 bem mais rápidos.
- **Zink:** OpenGL por cima do Vulkan.
- **PipeWire** no lugar de só ALSA: menos atraso e áudio Bluetooth.
- **SDL3:** sucessor, com API de GPU moderna.
- **Shaders no ScrapCade:** desfoque real e brilho na capa (OpenGL ES direto).

---

## 7. ScrapCade, o programa próprio (C++17 + SDL2)

Código em `external/package/scrapcade/src/`:

| Arquivo | Função |
|---|---|
| `scrapcade.cpp` | Tela principal: carrossel, temas, filtros e painel Sistema (detalhes, BIOS, instalador, Wi-Fi, volume, configurar controle) |
| `gfx.h` | Desenho: degradês, cantos arredondados, textos, perspectiva das capas |
| `catalogo.h` | Consoles, extensões e leitura das pastas de jogos |
| `meta.h` | Nome, ano, empresa e gênero pelo **CRC** (índices `crc/*.idx` e `fbneo.idx`) |
| `capas.h` | Baixa capa e fundo do acervo **libretro-thumbnails** (libcurl + OpenSSL) |
| `organizar.h` | Pasta "Jogos novos": zip, CD, Neo Geo, imagem de disco pelo cabeçalho |
| `envio.h` | Página web na porta 8080 (enviar jogos, trocar capa, apagar) |
| `arquivos.h` | Gerenciador de arquivos da página |
| `bios.h` | Confere as BIOS pelo **MD5** |
| `usb.h` | Importa jogos de pen drive |
| `rede.h` | Wi-Fi pelo ConnMan |

**Para pesquisar:**

- **ScreenScraper** (opcional, com conta): capas, vídeos e descrições.
- **Vídeo de prévia** do jogo no carrossel.
- **Coleções e listas** de jogos.
- **Senha na página web.**
- **Idiomas** (inglês).
- **Temas baixáveis.**
- **Modo para TV:** fontes maiores para ver de longe.

---

## 8. Emuladores

| Peça | Versão | Consoles |
|---|---|---|
| **RetroArch** | 1.22.2 | Frontend: KMS + OpenGL, ALSA, SDL2/udev, menu **RGUI** |
| Mesen | commit fixo | NES |
| Snes9x | commit fixo | Super Nintendo |
| Genesis Plus GX | commit fixo | Mega Drive, Master System, Game Gear |
| Gambatte | commit fixo | Game Boy e Game Boy Color |
| mGBA | commit fixo | Game Boy Advance |
| PCSX ReARMed | commit fixo | PlayStation (BIOS livre embutida) |
| Mupen64Plus-Next | commit fixo | Nintendo 64 |
| FBNeo | commit fixo | Fliperama e Neo Geo |

Configuração central: `/usr/share/tos/retroarch.cfg`. Ela define:

- Select + Start sai do jogo.
- O jogo salva sozinho e continua de onde parou.
- Controles no padrão SDL.

**Para pesquisar:**

- **Emuladores que faltam:**
  - PSP: PPSSPP.
  - Nintendo DS: melonDS.
  - GameCube e Wii: Dolphin.
  - PS2: PCSX2.
  - 3DS: Azahar.
  - PC antigo: DOSBox Pure e ScummVM.
- **Perfis por máquina:** mostrar só os consoles que o PC aguenta.
- **Shaders CRT** (slang) e **run-ahead** por console.
- **RetroAchievements** e **netplay**.
- **DuckStation** para PS1, mais preciso.

---

## 9. Modo desktop (só abre quando pedido)

| Peça | Função |
|---|---|
| **X.org** + driver modesetting + libinput | Base gráfica do desktop |
| **Fluxbox** | Gerenciador de janelas (estilo antiX) |
| **NetSurf 3.10** (GTK3) | Navegador; os downloads vão para "Jogos novos" |
| **PCManFM 1.3.2** | Arquivos |
| **xterm, nano** | Terminal e editor de texto |
| **mpv** (+ ffmpeg) | Vídeo e música |
| **feh, MuPDF** | Imagens e PDF |
| **GTK3 + Adwaita** | Visual e ícones dos programas |

**Para pesquisar:**

- **Wayland com labwc ou cage** no lugar de X + Fluxbox: mais moderno.
- **Navegador com JavaScript:** o NetSurf não roda sites modernos. As opções (Falkon, WebKitGTK) são bem mais pesadas.
- **LibreOffice:** não existe no Buildroot; seria preciso empacotar ou usar AppImage.

---

## 10. Tamanhos

| Item | Tamanho |
|---|---|
| Sistema (`tos.squashfs`) | ~250 MB (cresce com os firmwares de Wi-Fi) |
| `.iso` | ~250 MB |
| `disk.img` | 1 GB (a TOS-BOOT; o resto do pen drive vira TOS-DADOS) |
| Memória usada no menu | cerca de 100 a 150 MB |

---

## Onde pesquisar

- **Batocera** (`github.com/batocera-linux/batocera.linux`): o mais parecido com o T-OS. Mostra como empacotar emuladores, Bluetooth e Vulkan no Buildroot.
- **Lakka** e **ROCKNIX**: focados em leveza e em TV boxes e portáteis ARM.
- **Manual do Buildroot:** `buildroot.org/manual`.
- **Documentação do RetroArch:** `docs.libretro.com`.

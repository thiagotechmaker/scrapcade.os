# ScrapCade: Rumo ao Steam Deck — Programas, Bibliotecas e Linguagens

*Levantamento técnico das camadas que faltam para o ScrapCade funcionar como console (ScrapCade) e também como desktop completo, no nível do SteamOS/Steam Deck.*

Este documento junta o que já está definido nos dois documentos de arquitetura do ScrapCade com o que foi levantado nas discussões sobre universalidade de hardware e modo desktop. Cada tabela traz: a peça, pra que serve, em que linguagem ela é escrita (a maioria são projetos prontos — você integra, não reescreve) e se já está no seu plano ou é novo.

## 1. Build e automação

| Peça | Função | Linguagem | Status |
| --- | --- | --- | --- |
| Buildroot 2026.02.3 LTS | monta o sistema do zero | Make, Python | já em uso |
| Toolchain Bootlin (GCC 14.3.0, glibc) | compila tudo pra x86\_64/i686 | C, C++ | já em uso |
| ccache | acelera recompilação | C | já previsto |
| GitHub Actions | compila ISO/IMG 32 e 64-bit em paralelo na nuvem | YAML | já previsto |
| genimage / xorriso | monta disk.img e .iso | C | já em uso |
| QEMU (SeaBIOS/OVMF) | testa BIOS e UEFI sem hardware real — ideal pra automação (GitHub Actions, sem interface gráfica) | C | já em uso |
| **Scripts de build multi-nível (v1/v3)** | gera binários otimizados por CPU, além do baseline universal | Bash ou Python | novo |
| VirtualBox | testes manuais do menu gráfico EFI/BIOS — roda mais fluido que o QEMU nisso, segundo quem compara os dois, e facilita testar controle USB e áudio | C++ | novo |
| Bochs | depuração instrução por instrução do próprio bootloader/BIOS — bem mais lento, só pra esse caso específico | C++ | novo — opcional |

QEMU continua certo pro teste automatizado (GitHub Actions, sem interface gráfica, scriptável) — essa parte não muda. Mas pra testar manualmente a tela de boot e o menu gráfico, vale ter o VirtualBox por perto: tem suporte EFI embutido e, segundo quem usa os dois lado a lado, roda as telas gráficas do UEFI mais fluido que o QEMU em várias máquinas, além de facilitar testar controle USB e áudio por padrão. O Bochs fica só pra quando precisar depurar o próprio bootloader/BIOS passo a passo — é bem mais lento, mas mostra cada instrução.

&#91;embedded content: typed in chat · faixas de overhead de CPU/RAM levantadas em pesquisa (QEMU/KVM vs VirtualBox)\]

Por isso o QEMU continua sendo a escolha certa pro teste do dia a dia, deixando o VirtualBox só pro teste manual ocasional da tela gráfica.

## 2. Boot e atualização segura

| Peça | Função | Linguagem | Status |
| --- | --- | --- | --- |
| Syslinux/Isolinux 6.03 | menu BIOS legado | C, Assembly | já em uso |
| GRUB 2.12 | menu UEFI | C | já em uso |
| memtest86+ | teste de memória | C, Assembly | já em uso |
| **shim** (pacote `boot/shim` do Buildroot) | assina GRUB pra Secure Boot, com chaves Microsoft | C | novo |
| **EFI Boot Guard** | atualização em slots A/B com rollback automático se o update falhar | C | novo |
| Plymouth ou psplash | splash animado no boot | C | pesquisa futura já listada |

## 3. Kernel, CPU e drivers

| Peça | Função | Linguagem | Status |
| --- | --- | --- | --- |
| Linux 6.12.47 LTS (PREEMPT, 1000Hz) | kernel | C | já em uso |
| **intel-ucode / amd-ucode** | microcódigo da CPU, carregado no initrd antes do resto do boot | firmware binário | novo |
| CONFIG\_HID\_PLAYSTATION / CONFIG\_HID\_NINTENDO | suporte nativo a DualSense/DualShock e Joy-Con/Pro Controller | C (kernel) | novo — conferir se está habilitado no fragment |
| **xpadneo** (módulo fora da árvore) | Xbox Wireless e 8BitDo via Bluetooth, com layout de botões correto | C | novo |
| linux-firmware | firmware de Wi-Fi/Bluetooth/GPU | binário | já em uso — revisar cobertura |

## 4. Armazenamento e persistência

| Peça | Função | Linguagem | Status |
| --- | --- | --- | --- |
| OverlayFS | une o squashfs (somente leitura) com a camada gravável | C (kernel) | já em uso |
| exfatprogs, dosfstools | formata SCRAP-DADOS e SCRAPCADE | C | já em uso |
| **Partição de dados para apps** (ext4 ou btrfs) | guarda Flatpak (`/var/lib/flatpak`, `~/.var/app`) e a home do usuário | — | novo |

## 5. Serviços de sistema e conectividade

| Peça | Função | Linguagem | Status |
| --- | --- | --- | --- |
| BusyBox, eudev, D-Bus, util-linux | base do sistema | C | já em uso |
| ConnMan + wpa\_supplicant | rede por cabo e Wi-Fi | C | já em uso |
| bluez5\_utils (BlueZ) | pareamento Bluetooth | C | já planejado |
| samba4 | compartilha ROMs com Windows na rede local | C | já planejado |
| dropbear | SSH leve, desativado por padrão | C | já planejado |
| **xdg-desktop-portal + backend wlroots** | dá permissão de arquivo/câmera/microfone pros apps Flatpak | C | novo |

## 6. Gráficos, áudio e compositor de sessão

| Peça | Função | Linguagem | Status |
| --- | --- | --- | --- |
| Mesa 26.0.1 (iris, radeonsi, nouveau, llvmpipe) | drivers gráficos | C, C++ | já em uso |
| **Zink** | OpenGL rodando sobre Vulkan — cobre GPUs sem driver OpenGL nativo bom | C | novo |
| RADV (AMD) / ANV (Intel) | drivers Vulkan | C | já planejado |
| ALSA | áudio base | C | já em uso |
| **PipeWire + WirePlumber** | áudio com suporte nativo a Bluetooth, sem quebrar os emuladores | C | pesquisa futura → priorizar |
| **gamescope** | compositor único: modo kiosk embarcado e modo aninhado no desktop, com upscaling FSR/NIS e VRR | C++ | novo |

## 7. ScrapCade — frontend do modo console

| Peça | Função | Linguagem | Status |
| --- | --- | --- | --- |
| SDL2 / SDL2\_image / SDL2\_ttf | janela, controle, capas, texto | C | já em uso |
| SDL\_GameControllerDB | banco de controles da comunidade | dados | já em uso |
| libcurl + OpenSSL | baixa capas do libretro-thumbnails | C | já em uso |
| **libcurl + libxml2 (API ScreenScraper)** | scraping nativo de capas, vídeos e descrições | C/C++ | pesquisa futura já listada |
| **SDL3** | sucessor do SDL2, com API de GPU moderna | C | pesquisa futura já listada |

## 8. Modo desktop — ambiente e distribuição de apps

| Peça | Função | Linguagem | Status |
| --- | --- | --- | --- |
| Wayland + Labwc | compositor do modo desktop | C | já planejado, substitui X11/Fluxbox |
| **Flatpak** | instala qualquer app (LibreOffice, GIMP, navegadores) isolado do sistema base — funciona mesmo o Buildroot não tendo gerenciador de pacotes nativo | C | novo — peça central do "fazer o que quiser" |
| **AppImage** | apps avulsos fora do Flathub, sem daemon nem instalação | C | novo — complementa o Flatpak |
| Midori ou Falkon | navegador com JavaScript moderno, substitui o NetSurf | C++ | já planejado |
| **CUPS** | impressão e scanner | C | novo — não estava no plano original |
| mpv, feh, MuPDF | vídeo, imagem, PDF | C | já em uso |

## 9. Jogos modernos e emuladores pesados

| Peça | Função | Linguagem | Status |
| --- | --- | --- | --- |
| **Steam** (instalado via Flatpak) | loja e cliente de jogos | C++ | novo |
| **Proton / Proton-GE** | roda jogos Windows nativos (Wine mantido pela Valve) | C | novo |
| PPSSPP, Dolphin, PCSX2 | PSP, GameCube/Wii, PS2 — dependem do Vulkan já planejado | C++ | já planejado |
| RetroArch 1.22.2 + núcleos libretro | emulação retrô central | C | já em uso |

## 10. Controles e periféricos

| Peça | Função | Linguagem | Status |
| --- | --- | --- | --- |
| hid-sony / hid-playstation | DualShock 4 e DualSense | C (kernel) | novo — já mainline, só habilitar |
| hid-nintendo | Joy-Con e Pro Controller | C (kernel) | novo — já mainline, só habilitar |
| xpadneo | Xbox Wireless e 8BitDo via Bluetooth | C | novo (ver seção 3) |
| udev rules de permissão | dá acesso ao usuário sem precisar de root | regras de texto | novo |

## 11. Linguagem recomendada para o código próprio do ScrapCade

As peças acima são projetos prontos — você as integra como estão. A escolha de linguagem importa mesmo é no código que é seu:

| Código próprio | Linguagem recomendada | Por quê |
| --- | --- | --- |
| ScrapCade (frontend) | **C++17** (já escolhido) | acesso direto a SDL2/DRM/KMS, zero overhead — manter |
| Scripts de init, hardware probe, troca de sessão (kiosk ↔ desktop) | **Bash / POSIX shell** | já é o padrão em `/etc/init.d` e `/usr/bin` do ScrapCade; não vale introduzir outra linguagem só pra isso |
| Lógica de atualização OTA com slots A/B | **Rust** | é código crítico — se corromper, o sistema não boota mais; memory safety ajuda bastante aqui. C também funciona, se preferir manter tudo no mesmo ecossistema do kernel |
| Scraping nativo via API REST (ScreenScraper) | **C++**, dentro do próprio ScrapCade | evita depender de um processo externo e de outro runtime |
| Scripts de build e CI (multi-arch, detecção de hardware) | **Python** ou **Bash** | GitHub Actions roda bem com os dois; Python ajuda se a lógica de detecção crescer além de um script simples |
| Servidor web do ScrapCade (porta 8080) | **C++**, como já está em `envio.h` | trocar de linguagem aqui só adicionaria uma dependência de runtime extra (Node, Python) num sistema que preza por ser enxuto |

## 12. Resumo: o que já estava no plano vs. o que é novo

- **Já definido nos seus documentos de arquitetura**: toda a base das seções 1 a 7 do "Documento de Arquitetura e Roadmap", incluindo as melhorias que você já tinha como pesquisa futura (Bluetooth via BlueZ, Samba, PipeWire, Vulkan, SDL3, Secure Boot, atualização OTA).
- **Novo, para fechar no nível Steam Deck**: gamescope como compositor único entre console e desktop; Flatpak + AppImage como camada de instalação de apps; Steam/Proton como jogos modernos; CUPS para impressão; xpadneo/hid-playstation/hid-nintendo para os controles mais usados pela comunidade; EFI Boot Guard + shim para atualização segura em slots A/B; e microcódigo de CPU (intel-ucode/amd-ucode) para estabilidade em hardware mais antigo.

## 13. Núcleos de emulação recomendados (pesquisa atualizada)

Conferindo o consenso atual da comunidade RetroArch/libretro por sistema. A maioria confirma o que você já tinha — os destaques em **negrito** são mudanças ou adições à sua lista:

| Sistema | Núcleo recomendado | Alternativa | Nota |
| --- | --- | --- | --- |
| NES/Famicom | Mesen | — | já é o mais preciso, manter |
| SNES/Super Famicom | Snes9x (já tem) | **+ bsnes** | bsnes é o mais preciso que existe hoje, mas pesa mais; Snes9x continua certo pra hardware fraco — oferecer os dois, não trocar |
| Mega Drive/Master System/Game Gear | Genesis Plus GX | — | continua o padrão, manter |
| Game Boy/Color | Gambatte (já tem) | **+ SameBoy** | SameBoy ganhou em correção de cor nos últimos anos; comunidade recomenda os dois lado a lado |
| Game Boy Advance | mGBA | — | continua o padrão-ouro, manter |
| PlayStation | **DuckStation** (trocar) | PCSX ReARMed | virou a recomendação padrão da maioria dos guias de 2026, builds contínuos, já roda nativo em Steam Deck |
| Nintendo 64 | Mupen64Plus-Next | — | continua o mais equilibrado, manter |
| Nintendo DS | **melonDS** (fechar a pesquisa futura) | — | confirmado como melhor que DeSmuME |
| Nintendo 3DS | **Azahar** (fechar a pesquisa futura) | — | sucessor oficial do Citra, descontinuado em 2024; confirmado ativo em outubro/2026 |
| Arcade/Neo Geo | FBNeo | — | continua recomendado, prioriza jogabilidade sem perder precisão |
| **Sega Saturn** | **Beetle Saturn** | Kronos (ainda em desenvolvimento) | sistema que não estava em nenhum dos dois documentos |
| **Dreamcast/NAOMI/Atomiswave** | **Flycast** | — | cobre três sistemas Sega com um núcleo só |
| PSP | PPSSPP | — | já planejado, praticamente a única opção necessária |
| GameCube/Wii | Dolphin | — | já planejado |
| PS2 | PCSX2 | — | já planejado |
| **PlayStation 3** | **RPCS3** | — | não estava em nenhum dos dois documentos; builds contínuos, nativo em Linux, mas exige CPU/GPU fortes — bom candidato pro "filtro por máquina" que você já tinha planejado, escondendo em hardware fraco |

## 14. Notebooks: detecção de bateria e percentual

| Peça | Função | Linguagem | Status |
| --- | --- | --- | --- |
| CONFIG\_ACPI, CONFIG\_ACPI\_BATTERY, CONFIG\_ACPI\_AC | expõe bateria e estado de energia (CA/bateria) no kernel | C (kernel) | novo — conferir no fragment do kernel |
| `/sys/class/power_supply/BAT0/capacity` | percentual direto da bateria (0–100), sem precisar de daemon | leitura de arquivo | novo — caminho mais leve, ideal pro ScrapCade ler direto |
| `/sys/class/power_supply/BAT0/status` | estado: Charging / Discharging / Full / Not charging | leitura de arquivo | novo |
| **upower** (D-Bus) | abstração padrão com estimativa de tempo restante e eventos de bateria fraca | C | novo — opcional, só pro modo desktop |

Como encaixar no ScrapCade: pro ScrapCade (modo console), o caminho mais enxuto — e mais coerente com a filosofia do projeto — é um módulo próprio (um `bateria.h`, no mesmo estilo de `bios.h`/`capas.h`) que só lê esses dois arquivos de `/sys/class/power_supply/BAT0/` direto, sem precisar de upower nem D-Bus. Pro modo desktop, aí vale ativar o upower, porque os aplicativos GTK3 (Falkon, PCManFM) já esperam esse serviço rodando pra mostrar ícone de bateria.

Dá pra aproveitar essa detecção além de só mostrar o percentual: o script de governador de CPU que você já tem planejado pode checar `/sys/class/power_supply/AC/online` — se estiver em `0` (rodando na bateria), força o governador pra economia mesmo dentro do RetroArch, em vez de deixar sempre em modo "performance" só porque um jogo abriu.

## 15. Modos de energia selecionáveis (economia / moderado / máximo)

Isso formaliza o "script de governador de CPU" que você já tinha planejado, virando um menu real com três modos escolhidos pelo usuário — no painel Sistema do ScrapCade e também no modo desktop — em vez de só alternar sozinho quando abre ou fecha o RetroArch.

A peça certa aqui é o **TLP**, não o power-profiles-daemon (PPD): o PPD é o padrão em GNOME/KDE, mas pensado pra rodar com systemd; o TLP, desde a versão 1.11, já traz um **script de inicialização SysVinit** pronto — o mesmo estilo que o ScrapCade já usa em `/etc/init.d` — e tem de fábrica os três perfis: `performance`, `balanced` e `power-saver`, mapeando direto pros seus três modos.

| Modo pedido | Perfil TLP | Governador de CPU | EPP (energy performance preference) | Turbo Boost |
| --- | --- | --- | --- | --- |
| **Economia de bateria** | power-saver | powersave | power | desligado |
| **Moderado** | balanced | schedutil | balance\_performance | ligado, sem forçar |
| **Máximo** | performance | performance | performance | sempre ligado |

| Peça | Função | Linguagem | Status |
| --- | --- | --- | --- |
| **TLP** | aplica os três perfis via sysfs (governador, EPP, boost, Wi-Fi/USB power save) | Shell/Perl | novo |
| **tlp-pd** | expõe a mesma interface D-Bus do power-profiles-daemon, pra um painel no modo desktop (Labwc) ler/mudar o modo do jeito padrão que apps GTK já esperam | C | novo — só necessário no modo desktop |
| CONFIG\_X86\_AMD\_PSTATE / CONFIG\_X86\_INTEL\_PSTATE | drivers modernos de frequência com suporte a EPP, mais granular que só trocar o governador clássico | C (kernel) | novo — conferir no fragment, complementa o `CONFIG_CPU_FREQ` que você já tem |

Como ligar com o que já está planejado: o RetroArch continua forçando "Máximo" sozinho ao abrir um núcleo pesado (PCSX2, Dolphin, RPCS3) e volta pro modo escolhido pelo usuário ao fechar — só que agora esse "modo escolhido" pode ser Economia ou Moderado, não só um governador fixo. No painel Sistema do ScrapCade, a troca de modo chama o TLP direto; o D-Bus/`tlp-pd` só entra em cena pro modo desktop.

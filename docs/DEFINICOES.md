# T-OS — definições do sistema Linux (11/11/2026)

Sistema próprio, baseado em Linux e montado com **Buildroot**, voltado para emulação de jogos e para dar uma segunda vida a equipamentos que iriam para o lixo. Menu próprio: **ScrapCade**.

## Plataformas

| Item | Decisão |
|---|---|
| Arquiteturas | x64 (x86_64), x86 32 bits (i686), Raspberry Pi, outras ARM (TV boxes) |
| RAM mínima | 2 GB |
| Vídeo | Só drivers abertos (Mesa: Intel, AMD, Nouveau para NVIDIA) |
| Telas | Monitores e TVs HDMI, telas de notebook (com brilho), TVs de tubo (CRT, saída 240p) |
| Rede | Cabo e Wi-Fi (firmwares de Wi-Fi redistribuíveis incluídos) |

## Uso e interface

| Item | Decisão |
|---|---|
| Uso | Só jogos, com **modo desktop** acessível quando precisar |
| Modo desktop | **Fluxbox**, no estilo do antiX: PCManFM (arquivos), xterm (terminal), Dillo (navegador leve), mpv (vídeo), feh (imagens). O XFCE não existe no Buildroot. |
| Menu de jogos | **ScrapCade novo do zero**, em C/C++ com SDL2 (estilo Aurora, carrossel de capas) |
| Idioma | Português do Brasil, com opção de inglês |
| Contas | Sem conta no modo jogo; senha só no modo desktop e no acesso remoto |
| Menu durante o jogo | **Select + Start** abre o menu rápido (salvar, carregar, trocar disco, sair) |
| Tela de boot | Logo fixa do T-OS, sem textos do Linux |
| Tempo de boot | Meta de até 15 segundos até o menu |

## Jogos e emuladores

| Console | Emulador | BIOS |
|---|---|---|
| NES, SNES, Mega Drive, Master System | RetroArch (Mesen, Snes9x, Genesis Plus GX) | Não precisa |
| GB, GBC, GBA | mGBA | Livre embutida |
| Arcade | FBNeo | Não precisa (maioria) |
| Neo Geo | FBNeo | **Do usuário** (`neogeo.zip`) |
| PS1 | DuckStation (PC) / PCSX ReARMed (ARM) | Livre no PCSX ReARMed; original do usuário opcional |
| N64 | Mupen64Plus-Next | Não precisa |
| PS2 | PCSX2 | **Do usuário** (não existe livre) |
| GameCube e Wii | Dolphin | Não precisa |
| PSP | PPSSPP | Não precisa |
| Nintendo DS | melonDS | Livre embutida |
| 3DS | Azahar (sucessor do Citra) | Não precisa (jogos descriptografados) |
| PC antigo | DOSBox-Pure, ScummVM, ports nativos (Doom, Quake) | Não precisa |

- **BIOS:** o T-OS só distribui BIOS livres. Aceita a BIOS que o usuário tiver do próprio console, e avisa quais faltam.
- **Controles:** USB (Xbox, PlayStation, genéricos), Bluetooth sem fio, teclado e mouse, painéis de fliperama (encoders USB).
- **Capas e dados:** acervo da libretro (sem conta) e ScreenScraper opcional (com a conta do usuário).
- **Conquistas:** RetroAchievements opcional, desligado por padrão.
- **Saves:** no aparelho e visíveis na pasta compartilhada da rede.

## Arquivos e transferência

| Item | Decisão |
|---|---|
| Partição dos jogos | exFAT (o Windows lê e grava direto) |
| Transferência | Página no navegador, pasta compartilhada (Samba), pen drive com importação automática, SSH (desligado por padrão) |

## Instalação, versões e distribuição

| Item | Decisão |
|---|---|
| Instalação | Roda do pen drive (para testar) e tem instalador para o HD |
| Atualização | Manual: baixar a imagem nova e gravar de novo |
| Versões | Por data (ex.: `2026.11`) |
| Site de download | Domínio próprio (nome e custo a definir); arquivos nas GitHub Releases |
| Licença | GPLv3 para o T-OS e o ScrapCade |

## Pontos de atenção

1. **Consoles pesados em máquinas fracas:** PS2, GameCube/Wii e 3DS exigem processador de 2 núcleos bons e OpenGL 3.3 ou Vulkan. Em x86 32 bits, TV boxes e Raspberry Pi antigos não vão rodar. O sistema deve mostrar só os consoles que a máquina aguenta (perfis por desempenho).
2. **NVIDIA só com driver aberto (Nouveau):** placas NVIDIA mais novas ficam lentas para 3D.
3. **CRT 240p:** depende de placa com saída analógica (VGA/componente) e do cabo certo. Fica como recurso avançado.
4. **TV boxes:** cada modelo precisa de kernel e device tree próprios. Começar pelos mais comuns.
5. **Domínio próprio:** escolher o nome e registrar (custo anual). Até lá, as imagens ficam nas GitHub Releases.
6. **Firmwares de Wi-Fi:** alguns são proprietários, mas redistribuíveis. Ficam fora do código GPL, em pacote separado.

## Ordem de trabalho

1. Ambiente de compilação (Buildroot no WSL2 com Ubuntu).
2. Imagem x64 mínima: liga pelo pen drive e mostra a logo e uma tela de teste.
3. RetroArch e os primeiros núcleos (8/16 bits, PS1, N64). O RetroArch não existe no Buildroot: o T-OS empacota ele e os núcleos, como o Batocera.
4. ScrapCade em SDL2: carrossel, capas, organização por console.
5. Transferência (página, Samba, pen drive) e menu Select + Start.
6. Imagem x86 32 bits, depois Raspberry Pi, depois TV boxes.
7. Emuladores pesados (PCSX2, Dolphin, PPSSPP, melonDS, Azahar) com perfis por desempenho.
8. Modo desktop Fluxbox e instalador para o HD.
9. Site de download.

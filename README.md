# ScrapCade OS

<p align="center"><img src="docs/logo.png" width="620" alt="ScrapCade OS"></p>

<p align="center"><b>Jogos em qualquer computador.</b><br>
Sistema próprio, baseado em Linux, para <b>emulação de jogos</b> e para dar uma <b>segunda vida a computadores que iriam para o lixo</b>.</p>

<p align="center"><a href="https://github.com/thiagotechmaker/scrapcade.os/releases/latest"><b>⬇️ Baixar a versão mais nova (.iso, PC 64 bits)</b></a></p>

## Baixar e instalar

1. **Baixe** o `scrapcade-os-<versão>-pc-64bits.iso` em [Releases](https://github.com/thiagotechmaker/scrapcade.os/releases/latest). É um arquivo só, que liga por BIOS antigo e por UEFI.
2. **Grave** num pen drive de 4 GB ou mais com o [Rufus](https://rufus.ie) (modo ISO ou DD, tanto faz) ou o [balenaEtcher](https://etcher.balena.io).
3. **Ligue o PC pelo pen drive** (tecla do menu de boot: F12, F9, F8 ou Esc). No primeiro boot o ScrapCade OS arruma o pen drive sozinho (sistema + partição SCRAP-DADOS para os jogos), pede a senha do administrador e abre a criação do seu perfil.
4. **Coloque jogos** (veja [Colocar jogos](#colocar-jogos)).
5. **Quer ligar sem o pen drive?** O ScrapCade OS pergunta uma vez se quer instalar no HD/SSD (também em *Sistema > Instalar no disco do computador*).

As versões novas chegam sozinhas: o ScrapCade OS avisa na tela e atualiza em *Sistema > Atualizar o ScrapCade OS*, sem mexer nos jogos e saves. Jogos, BIOS e ROMs não acompanham o sistema.

## Como é

Liga direto no **modo jogo**, o menu **ScrapCade**: um carrossel de capas, com filtros por console, favoritos, "Continuar jogando" e os dados de cada jogo. Nas capturas aparecem jogos de fãs com licença livre ([µCity](https://github.com/AntonioND/ucity) de AntonioND, GPLv3; [Nova the Squirrel](https://github.com/NovaSquirrel/NovaTheSquirrel) de NovaSquirrel, GPLv3; [Libbet and the Magic Floor](https://github.com/pinobatch/libbet) de Pino, zlib), com capas feitas das telas dos próprios jogos.

![Carrossel](docs/capturas/5_menu_carrossel.png)

Cada pessoa tem o seu **perfil** (avatar, favoritos e saves próprios). Ao ligar, ele pergunta "Quem vai jogar?".

![Perfil](docs/capturas/10_perfil_avatar.png)

No **painel Sistema** (botão Select ou tecla Tab) fica tudo, em seções como o menu de ajustes de um console:

- **Jogos:** perfil, jogos grátis, trazer do pen drive, gerenciador de arquivos, ejetar o pen drive, ordenar jogos, capas, BIOS, cópia dos saves.
- **Jogando:** voltar de onde parou, filtro de imagem (liso ou TV de tubo), proporção, menos atraso nos controles, FPS na tela, emulador de cada console.
- **Aparência:** 8 temas ou um seu, tamanho do texto, animações, proteção de tela.
- **Controle e som, Tela e energia, Rede e Sistema:** controle, volume, Bluetooth, energia, desligar sozinho, relógio, Wi-Fi, pasta na rede do Windows, atualizar, instalar no HD, voltar ao padrão.

Do lado direito ele explica o que cada opção faz.

![Painel Sistema](docs/capturas/11_painel_sistema.png)

**8 temas** prontos, ou um tema seu: escolha qualquer imagem em *Sistema > Tema* (botão A) e ela vira o fundo.

![Tema](docs/capturas/7_menu_tema.png)

**Jogos grátis:** em *Sistema > Jogos grátis* o ScrapCade baixa jogos de fãs com licença livre (µCity, Nova the Squirrel, Libbet e o Freedoom, um Doom completo e livre), direto do site do autor, confere o arquivo e já coloca no console certo, com capa. Os **ports** (jogos de PC adaptados) vêm pelo **PortMaster**, no console *Ports*.

![Jogos grátis](docs/capturas/12_jogos_gratis.png)

**Gerenciador de arquivos** (*Sistema > Gerenciador de arquivos*): abre direto nas pastas de jogos; Y mostra as ações (copiar, mover, clonar, renomear, apagar para a Lixeira) e X cola na pasta aberta.

Se um jogo não abrir (arquivo com defeito, versão errada, falta de BIOS), o ScrapCade avisa na tela em vez de só voltar ao menu.

## Colocar jogos

- **Pelo navegador** do PC ou do celular, na mesma rede: abra o endereço que aparece na tela e arraste os jogos.
- **Pela rede do Windows:** a pasta dos jogos aparece no Explorador de Arquivos.
- **Por um pen drive:** *Sistema > Trazer jogos do pen drive*.

Os jogos vão sozinhos para o console certo, com capa e dados. Jogos, BIOS e ROMs não acompanham o sistema.

## Consoles

NES, Super Nintendo, Mega Drive, Master System, Game Gear, Game Boy, Game Boy Color, Game Boy Advance, PlayStation, Nintendo 64, PSP, Nintendo DS, Sega Saturn, Dreamcast, Fliperama (FBNeo) e Neo Geo, pelo RetroArch. Também Doom e jogos `.wad` (PrBoom), **Half-Life** (Xash3D: copie a pasta `valve` da sua cópia da Steam para `roms/halflife`) e os ports do PortMaster.

**Atalhos em todo jogo:** Select + Start sai, Select + X abre o menu do emulador. No teclado: Esc sai.

## As duas versões do ScrapCade

| | **ScrapCade OS** (este repositório) | **Fliperama iChroma** |
|---|---|---|
| Máquina | PCs e notebooks de 64 bits (32 bits em breve) | Placa ARM de 1 núcleo a 1 GHz (iChroma II, Android 2.3), tirada do lixo eletrônico |
| Sistema | Linux próprio, montado com o Buildroot | Android de fábrica, com kernel e boot próprios |
| Menu | ScrapCade em C++ e SDL2 | ScrapCade em Java (o original) |
| Emuladores | Todos os que o PC aguentar | Só os leves (até PlayStation 1 e Nintendo 64, com limites) |

Os dois menus são o mesmo por desenho: carrossel de capas, painel Sistema, gerenciador de arquivos, jogos pelo navegador e pelo pen drive. O que nasce num é levado para o outro, dentro do que cada máquina aguenta. O fliperama do iChroma é um projeto pessoal, sem repositório público.

## Mais informações

- [Detalhes técnicos](docs/DETALHES.md): emuladores, partições, gravação, versões, modo desktop.
- [Do que o ScrapCade OS é feito](docs/COMPOSICAO.md): cada programa, com versões.
- [Roteiro](docs/ROTEIRO.md): o que vem por aí.

**Compilar** (Ubuntu ou WSL2; o ScrapCade OS é montado com o [Buildroot](https://buildroot.org), como o Batocera):

```bash
git clone https://github.com/thiagotechmaker/scrapcade.os.git ~/t-os
git clone --depth 1 --branch 2026.02.3 https://gitlab.com/buildroot.org/buildroot.git ~/t-os/buildroot
bash ~/t-os/build-x86_64.sh
```

## Licença

Código do ScrapCade OS: **GPLv3** (ver [LICENSE](LICENSE)). Os arquivos em `external/board/x86_64/` vêm da configuração de PC do Buildroot (GPL-2.0 ou posterior). Os programas incluídos na imagem mantêm as licenças dos seus autores.

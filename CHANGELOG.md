# Changelog

Todas as mudanças importantes do ScrapCade OS ficam aqui. O formato segue o [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/) e as versões seguem o [Versionamento Semântico](https://semver.org/lang/pt-BR/) (fase beta enquanto for `0.x`).

Até a v0.19.0-beta as versões eram publicadas com a data e a hora da compilação como tag (por exemplo `v202611151519`); em 15/11/2026 essas Releases foram renomeadas para o número SemVer (`v0.19.0-beta`, `v0.17.0-beta`, `v0.16.0-beta`), e da v0.20.0-beta em diante a tag já nasce assim. Quem estiver na v0.19.0-beta ou antes precisa gravar o `.iso` novo uma vez, porque o atualizador antigo só reconhece tags de data.

## [Unreleased]

## [0.20.0-beta] - 2026-11-15

### Adicionado
- Versão para PCs de 32 bits (i686): Pentium 4, Pentium M e Atom, com os emuladores leves (FCEUmm, Snes9x 2010, PicoDrive, gpSP) e vídeo Intel GMA 900/950/3150 (i915), AMD antigas (r300, r600) e NVIDIA (nouveau); teste automático próprio no QEMU (`tests/testar-32.sh`).
- Boot animado: as letras da logo aparecem uma a uma e uma barra de blocos corre embaixo até o sistema subir, como no fliperama do iChroma.
- "CARREGANDO..." animado ao abrir um jogo e na página do navegador durante envios e operações de arquivo.
- Troca rápida de jogo: o botão Start no carrossel abre "Continuar jogando", com a foto de onde você parou em cada jogo.
- Menu do botão de energia (Suspender, Reiniciar, Desligar). Jogando, o botão suspende; fechar a tampa do notebook também.
- Suspender no painel Sistema, voltando exatamente onde estava.
- Modo atração: a proteção de tela mostra as cenas dos jogos, com nome e console.
- Música (da pasta `music`) e sons no menu, desligados por padrão.
- PC antigo (DOS) pelo DOSBox Pure, direto do `.zip`.
- Tela de download do PortMaster no menu, com barra de progresso.
- Assistente de primeiro uso: depois do primeiro perfil, abre a Rede se o PC estiver sem conexão e diz se o controle está pronto.

### Alterado
- A atualização automática só avisa que há versão nova (baixar sozinho é uma opção do painel), e os sons do menu vêm ligados. A proteção de tela vem em 5 minutos.
- Um número de versão só (arquivo `VERSION`, em SemVer) no menu, em Informações, no `.iso` (`scrapcade-os-v<versão>-pc-64bits.iso`) e na tag.
- O atualizador compara pelo arquivo `BUILD_ID` publicado junto com a versão, e mostra a versão SemVer.
- Menu do RetroArch com as cores do ScrapCade.
- Imagem, `.iso` e atualização sabem a arquitetura: um PC de 32 bits só baixa os arquivos de 32 bits.

### Corrigido
- Nada de terminal na tela: o boot e o desligamento mandam as mensagens para um registro (`logs/boot.log`), e entre o menu e o jogo (na ida e na volta) aparece a logo com o CARREGANDO, desenhada direto na tela.
- Instalado no computador, o ScrapCade liga direto na logo; o menu de boot só aparece segurando Esc ou Shift (no pen drive ele continua aparecendo).
- O boot não pede mais a senha do administrador num terminal: ela é criada no painel, com o teclado na tela, ao ligar o SSH.
- Som: as saídas de todas as placas começam ligadas e em 80% (em muitos notebooks o alto-falante vinha mudo).
- Som pelas caixas e pelo fone, não pelo HDMI; áudio Intel pelo driver HDA clássico.
- Rede com cabo: se a placa não subir sozinha, o sistema liga a placa e pede o endereço ao roteador.
- Nintendo 64 em placas sem OpenGL 3.3 (Intel antigas): abre com o desenho pelo processador em vez de não abrir.
- Motor gráfico: a explicação muda conforme a opção escolhida (antes só falava do OpenGL ES e do software).
- Instalar no computador em PC de 32 bits com UEFI registrava o arquivo de boot de 64 bits.
- Scripts do sistema salvos no git sem permissão de execução: quem clonasse o repositório gerava uma imagem que não ligava.
- Informações e a tela de abrir jogo mostram o emulador que está instalado (na versão de 32 bits apareciam os de 64).

## [0.19.0-beta] - 2026-11-15

Publicada com a tag `v202611151519`, depois renomeada para SemVer.

### Adicionado
- Half-Life pelo Xash3D FWGS, com as bibliotecas do hlsdk-portable; os dados vêm da cópia da Steam do usuário (`roms/halflife/valve`).
- Painel Sistema em seções: Jogos, Jogando, Aparência, Controle e som, Tela e energia, Rede e Sistema.
- Ordenar jogos, consoles vazios, capas de novo, limpar "Continuar jogando", voltar de onde parou, filtro de imagem (nenhum, liso, TV de tubo), proporção, run-ahead, FPS na tela, emulador de cada console, relógio 12/24 h, voltar tudo ao padrão.
- Proteção de tela e desligar sozinho.

### Alterado
- Bateria de testes com segunda tentativa em todos os trechos do QEMU.

## [0.17.0-beta] - 2026-11-15

Publicada com a tag `v202611150727`, depois renomeada para SemVer.

### Adicionado
- Console Doom (PrBoom) e o Freedoom nos Jogos grátis, com capas feitas das telas dos próprios jogos.
- Aviso na tela quando um jogo não abre.
- Ejetar pen drive.

### Alterado
- Gerenciador de arquivos com o visual do fliperama do iChroma: abre nas pastas de jogos, Y mostra as ações (com Clonar) e X cola.

## [0.16.0-beta] - 2026-11-15

Publicada com a tag `v202611150307`, depois renomeada para SemVer.

### Adicionado
- PortMaster no console Ports.
- Gerenciador de arquivos, jogos grátis de licença livre, cópia dos saves no pen drive, "Continuar jogando".
- Atualização em segundo plano, que só avisa para reiniciar.
- Logo do ScrapCade OS no boot (BIOS e UEFI) e letras de pixel no tema 8 bits.

### Corrigido
- Duas atualizações ao mesmo tempo, confirmação da versão nova antes de ligar e download repetido.

## [0.15.0-beta] e anteriores - 2026-11-11 a 2026-11-14

Primeiras versões: sistema em Buildroot que liga por BIOS e UEFI, menu ScrapCade em C++/SDL2, perfis com avatar, temas, Wi-Fi, Bluetooth, Samba, SSH, BIOS dos consoles, instalador para o HD, .iso único, atualização A/B e bateria de testes automática.

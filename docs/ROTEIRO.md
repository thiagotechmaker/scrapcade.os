# Roteiro do ScrapCade OS

O que já entrou está no [CHANGELOG](../CHANGELOG.md). Aqui fica o que vem pela frente, na ordem combinada. Cada versão só é publicada depois de passar na bateria automática (`tests/testar.sh`).

## v0.23.0-beta (próxima)

- **ScreenScraper** opcional, com a conta do usuário (descrições, logos, vídeos). A API pede uma credencial de desenvolvedor do projeto, que ainda falta pedir ao ScreenScraper.
- **Jogar do CD ou DVD físico** (original ou gravado; pedido na [issue #1](https://github.com/thiagotechmaker/scrapcade.os/issues/1)): pôr o disco no leitor e ele aparecer no menu, pelo leitor de disco do RetroArch no PlayStation, Sega CD e PC Engine CD, e pelo PCSX2 no PS2. GameCube e Wii continuam por arquivo: a maioria dos leitores de DVD de PC não lê esses discos. O leitor de CD do PlayStation foi desligado na v0.22.1 (não deixava o emulador abrir) e precisa voltar compilado direito.
- **Organizador reconhece o console de um `.chd`**: hoje um `.chd` solto (pela página do celular ou fora da pasta do console no pen drive) vai sempre para o PlayStation, mesmo sendo de Sega CD, PC Engine CD, 3DO, Saturn, Dreamcast ou Neo Geo CD. Dentro de `roms/<console>/` no pen drive ele já vai certo.
- **Wii e 3DS num PC com placa de vídeo:** na máquina virtual o Wii fica carregando e o 3DS fica com a tela preta. E sair de um jogo travado no carregamento: o Select + Start só funciona depois que o emulador termina de abrir.
- **Consoles sem jogo no teste da v0.22.2** (Master System, PC Engine, Atari, ColecoVision, MSX, C64, Amiga e outros): testar com homebrew de licença livre.
- **Teste de cada console** com jogos de verdade (lista dos mais pesados por console no guia de testes), e ajuste dos que ficarem lentos.
- **Fliperama do iChroma:** importar de um pen drive de verdade, analógico direito e teclado/mouse no Half-Life (testes que pedem alguém do lado do aparelho); sons do menu, se o som da placa voltar.

## Depois

- Xbox 360 (Xenia), quando rodar jogos no Linux.
- Switch (Eden), só na versão de 64 bits e para PC forte; as chaves e o firmware vêm do console da própria pessoa (pedido na [issue #1](https://github.com/thiagotechmaker/scrapcade.os/issues/1)).
- Tela vertical, pistola de luz, spinner e trackball, TV de tubo em 240p, Moonlight.
- Fluidez: boot em até 15 segundos, abrir e sair dos jogos mais rápido.
- Teste em mais hardware de verdade (notebooks e PCs de 32 bits).
- Compilação na nuvem (GitHub Actions) e imagem otimizada para processadores de 2015 em diante (x86-64-v3).

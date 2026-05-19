# Configuração do Ambiente para Computação Gráfica com OpenGL sem instalações
Para usar nas máquinas dos laboratórios da Unisinos.

## Passo 1 - Compilador C/C++

Verificar se existe este diretório no computador da sala:
- C:\msys64
  - C:\msys64\ucrt64\bin

Se NÃO houver, precisa instalar o msys2:

Link: https://github.com/msys2/msys2-installer/releases/download/2025-12-13/msys2-x86_64-20251213.exe 

E então abrir o msys2 (Windows+R, digitar C:\msys64\ucrt64.exe e pressionar ENTER) e digitar no terminal:
```sh
pacman -S --needed base-devel mingw-w64-ucrt-x86_64-toolchain
```

- Primeira opção só dar ENTER
- Depois responder "yes" para tudo.

## Passo 2 - Baixar o CMAKE Portable no link abaixo:

Link: https://github.com/Kitware/CMake/releases/download/v4.0.2/cmake-4.0.2-windows-x86_64.zip

Descompactar (Sugere-se colocar em Documentos)

Ex:  C:\Users\NOME_DO_USUARIO\Documents\

## Passo 3 - Baixar Git portable no link abaixo:

Link: https://github.com/git-for-windows/git/releases/download/v2.49.0.windows.1/PortableGit-2.49.0-64-bit.7z.exe

Descompactar (clicar 2x)

## Passo 4 - Configurar o PATH do Windows

Abra o projeto no Visual Studio Code e então crie (caso não exista) a pasta .vscode dentro dele.

Depois, crie o arquivo settings.json, e dentro dele adicione:
```
{
    "cmake.cmakePath": "C:/Users/NOME_DO_USUARIO/Documents/cmake-4.0.2-windows-x86_64/bin/cmake.exe",
    "cmake.environment": {
        "PATH": "C:\\msys64\\ucrt64\\bin;C:\\Users\\NOME_DO_USUARIO\\Documents\\cmake-4.0.2-windows-x86_64\\bin;C:\\Users\\NOME_DO_USUARIO\\Documents\\PortableGit\\bin;${env:PATH}"
    },
    "cmake.buildEnvironment": {
        "PATH": "C:\\msys64\\ucrt64\\bin;C:\\Users\\NOME_DO_USUARIO\\Documents\\cmake-4.0.2-windows-x86_64\\bin;C:\\Users\\NOME_DO_USUARIO\\Documents\\PortableGit\\bin;${env:PATH}"
    }
}
```

Para garantir que deu tudo certo, aperte F1 e execute o comando **CMake: Delete Cache and Reconfigure**

⚠️⚠️Obs: modifique o caminho para o SEU USUÁRIO!!!⚠️⚠️

## Passo 5 - Abrir o VSCode e verificar se as seguintes extensões estão instaladas:

- C/C++
- C/C++ Extension Pack
- CMake Tools

## Passo 6 - Abrir o projeto no VSCode e configurar

Seguir a partir do passo **2. Clonando o Repositório de Exemplo** do tutorial: https://github.com/guilhermechagaskurtz/CGCCHibrido/blob/main/GettingStarted.md

## Passo 7 - Adicionar DLLs no projeto (ANTES DE COMPILAR E EXECUTAR)

Vá até a pasta **bin** de onde você descompactou o **msys2** (ou onde ele está instalado, por ex, C:\msys64\ucrt64\bin) e então copie as seguintes dlls para a pasta **build** do seu projeto:
- libstdc++-6.dll
- libgcc_s_seh-1.dll
- libwinpthread-1.dll

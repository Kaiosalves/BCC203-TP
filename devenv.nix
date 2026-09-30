{ pkgs, lib, config, inputs, ... }:

{
  # https://devenv.sh/packages/
  packages = [
    pkgs.git
    pkgs.clang
    pkgs.clang-tools
    pkgs.cmake
  ];

  # https://devenv.sh/languages/
  languages.cplusplus = {
    enable = true;
    lsp.package = pkgs.clang;
  };

  languages.python.enable = true;

  languages.texlive.enable = true;

  # Set compiler environment variables to Clang
  env.CXX = "clang++";
  env.CC = "clang";

  # https://devenv.sh/git-hooks/
  git-hooks.hooks = {
    clang-format.enable = true;
    clang-tidy.enable = true;
  };

  # https://devenv.sh/scripts/
  scripts = {
    build = {
      description = "Build C++ targets (pesquisa and generator) using CMake";
      exec = ''
        cmake -B build -S .
        cmake --build build "$@"
        if [ -f build/compile_commands.json ] && [ ! -e compile_commands.json ]; then
          ln -sf build/compile_commands.json compile_commands.json
        fi
      '';
    };

    generate = {
      description = "Generate binary files of Items using the generator target";
      exec = ''
        [ -f build/CMakeCache.txt ] || cmake -B build -S .
        cmake --build build --target generator
        mkdir -p tmp
        if [ $# -eq 0 ]; then
          ./build/generator -o tmp
        else
          ./build/generator "$@"
        fi
      '';
    };

    format = {
      description = "Format C++ source and header files using clang-format via CMake";
      exec = ''
        [ -f build/CMakeCache.txt ] || cmake -B build -S .
        cmake --build build --target format "$@"
      '';
    };

    "format-check" = {
      description = "Check formatting of C++ source and header files via CMake";
      exec = ''
        [ -f build/CMakeCache.txt ] || cmake -B build -S .
        cmake --build build --target format-check "$@"
      '';
    };

    tidy = {
      description = "Run clang-tidy on C++ source files via CMake";
      exec = ''
        [ -f build/CMakeCache.txt ] || cmake -B build -S .
        cmake --build build --target tidy "$@"
      '';
    };

    lint = {
      description = "Run linter (alias for tidy)";
      exec = ''
        [ -f build/CMakeCache.txt ] || cmake -B build -S .
        cmake --build build --target lint "$@"
      '';
    };

    clean = {
      description = "Remove build artifacts using CMake";
      exec = ''
        if [ "$1" = "--all" ]; then
          rm -rf build
        elif [ -f build/CMakeCache.txt ]; then
          cmake --build build --target clean "$@"
        else
          rm -rf build
        fi
      '';
    };
  };

  # See full reference at https://devenv.sh/reference/options/
}

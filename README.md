# PHX

A tool which can create and read disk images, format and update the partition table, and format and manipulate the filesystem.

PHX is in early development. In short: Nothing is guaranteed to be stable.

## Name

PHX stands for Phoenix.

## Installation

### Homebrew (macOS/Linux)

PHX is distributed via a custom Homebrew tap (third-party formula repository). You may use the full path to avoid potential future conflicts with other taps or Homebrew Core. As of the time of writing this README, there are no known conflicts with Homebrew Core. As of Homebrew 6.0.0, third-party taps must be explicitly trusted before their formulae can be loaded.

```sh
# Load the tap
brew tap JonathanMohr/tap

# Homebrew 6+:
brew trust JonathanMohr/tap # trust the whole tap
# or
brew trust --formula JonathanMohr/tap/phx # Only trust the formula

# Install PHX
brew install JonathanMohr/tap/phx
# or (if there are no conflicts)
brew install phx
```

### Scoop (Windows)

PHX is distributed via a custom Scoop bucket (third-party repository). You may use the full name to avoid potential future conflicts with other buckets or Scoop named buckets. As of the time of writing this README, there are no known conflicts with Scoop Core.

```sh
# Load the Scoop bucket (You can choose a different name)
scoop bucket add JonathanMohr https://github.com/JonathanMohr/scoop-bucket

# Install PHX
scoop install JonathanMohr/phx # Use the name you chose
# or (if there are no conflicts)
scoop install phx
```

### Build from source

If you decide to build manually, you will need the following tools available in your PATH:

- Python 3.10+
- clang, clang++
- lld, llvm-ar
- llvm-dsymutil or dsymutil (only required when targeting macOS)

```sh
# Clone and enter the repository
git clone https://github.com/JonathanMohr/phx
cd phx

# Build
python3 -m build
# or
python -m build
```

After building, `./.dist` will contain the install prefix (`/bin`, etc.). Copy its contents to a location of your choice (e.g. `~/.local/phx`), then either add its directories to you `PATH` (and possibly `MANPATH`, etc.), or link the necessary files and directories into a directory that's already on it.

## License

This project is licensed under Apache-2.0.
Files in `embed/` are additionally licensed under 0BSD.

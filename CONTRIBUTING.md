# Contributing Guide

So you want to contribute to JSON Voorhees?
I'd love your contribution!
Please help me.

## Building

Building the system only requires [CMake](https://cmake.org/) and the standard-issue C++ compilation tools.

### Local Builds

The authoritative build runs in [GitHub Actions](https://github.com/tgockel/json-voorhees/actions).
To run the same core build locally:

```bash
$> cd /path/to/json-voorhees
$> cmake -S . -B build -G Ninja -DJSONV_BUILD_TESTS=ON
$> cmake --build build --target check --parallel
```

Linux packages are built from the CMake install rules with CPack:

```bash
$> cmake -S . -B build-package -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
$> cmake --build build-package --target package --parallel
```

Windows NuGet packages use the Visual Studio generator:

```bash
$> cmake -S . -B build-package -G "Visual Studio 17 2022" -A x64 -DCPACK_GENERATOR=NuGet
$> cmake --build build-package --config Release --target package --parallel
```

macOS installer packages use CPack's ProductBuild generator:

```bash
$> cmake -S . -B build-package -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local -DCPACK_GENERATOR=productbuild
$> cmake --build build-package --target package --parallel
```

The CI package jobs install the generated packages and compile a small downstream program against the installed headers,
package metadata, and library.

## Process

This library follows the [GitHub Fork + Pull Model](https://help.github.com/articles/about-pull-requests/).
Below are the more project-specific steps.

### Issue Tracker

All work *must* be tracked in the [Issue Tracker](https://github.com/tgockel/json-voorhees/issues), otherwise the
maintainer will have no idea what is going on.
Try to find an existing bug in the list of issues -- if you can't find it, open a new issue with a descriptive title and
descriptive description.
If you are unclear on if it should be a bug or not, mark it with a *Question* tag or just send me an
[email](mailto:travis@gockelhut.com).
Assign the issue to yourself so I don't forget who is working on it.

For more granular tracking, the issue should move across the
[GitHub Project board](https://github.com/tgockel/json-voorhees/projects/1).
The columns of the project should be somewhat intuitive:

- **Backlog:** Not being actively worked on, but might be a good idea.
- **Design:** System is being designed. What this usually means is the API is being written. *Please* write your API
  first -- it can save a lot of time in the long run.
- **Implementation:** The component is currently being implemented.
- **Pull Request:** There is an open pull request.
- **Done:** Work is complete!

### Developing

1. Fork the repository.
2. Branch in your fork (not actually required, but generally considered a Good Idea).
3. Write your code.
4. If this is your first contribution, add yourself to `AUTHORS` (alphabetically).
5. Commit your code (somewhere in the commit message, be sure to mention "Issue #NN", where "NN" is the issue number you
   were working on).
6. Watch your tests pass in GitHub Actions.
7. Issue a pull request from your branch to the `trunk` branch of the main repository.
8. Close the branch in your repository (not actually required, but clean repos are nice).

#### Sign Your Commits

When committing code, please [sign commits with GPG](https://help.github.com/articles/signing-commits-using-gpg/).
This lets me know that work submitted by you was really created by you (security or something like that).
If you always want to sign commits instead of specifying `-S` on the command line every time, add it to your global
configuration:

```bash
$> git config --global user.signingkey ${YOUR_KEY_ID}
$> git config --global commit.gpgsign true
```

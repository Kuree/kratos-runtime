# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.0.8] - 2020-11-2
### Added
- Print out failed filename line number lookup into stdout
- Improve array handling

### Fixed
- Fix path translation for remote debugging

## [0.0.7] - 2020-02-07
### Added
- Add support for synch callback support
- Add remote debugging support

## [0.0.6] - 2020-01-27
### Added
- Add more functions to Python mock debugger
- Add simple tester runner to add additional flags and control threading
- Add state dumping
- Add coverage flags
- Add buildkite tests to test ncsim

### Changed
- Use forked remote of exprtk since the remote force pushes a lot
- Doesn't link with kratos anymore


## [Unreleased]
## [0.0.5] - 2019-12-11
### Fixed
- Fix wait till finish bug

## [0.0.4] - 2019-12-11
### Added
- Add status control
- Add python mock functions to poke simulator state

## [0.0.3] - 2019-12-10
### Added
- vcs flags to load kratos-runtime
- Add a python mock debugger for testing


## [0.0.2] - 2019-12-09
### Changed
- Refactor code as kratos remote changes

### Added
- fix a bug in context value handle calculation

## [0.0.1] - 2019-10-20
### Added
- Initial release.

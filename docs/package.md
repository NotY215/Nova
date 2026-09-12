# Vayu Package System

Vayu is intended to have a dedicated package ecosystem for distributing libraries, applications, native integrations, and reusable language components.

## Package Command

The planned command style is:

```text
nva install <package>
```

The package-manager command and implementation details may evolve as the compiler and ecosystem mature.

## Planned Capabilities

The package system is intended to provide:

- package installation
- dependency management
- version management
- project configuration
- lock files
- package publishing
- build integration
- native-library packages
- AI/ML packages
- GUI libraries
- game-development libraries

## Native Dependencies

A major goal is to make packages capable of exposing or consuming native libraries where appropriate. This is important for graphics, systems programming, AI/ML, scientific computing, and platform APIs.

## Ecosystem Direction

The long-term package workflow is intended to connect source packages with the Vayu compiler, build system, standard library, native toolchain, and future IDE/tooling ecosystem.

## Current Status

The package ecosystem is planned and under development. The existence of this document describes the project direction and does not imply that every package-management capability is already implemented.

For current project status and roadmap updates, visit the [official Vayu website](https://vayu.gt.tc).

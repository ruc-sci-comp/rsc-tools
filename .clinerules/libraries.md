# Overview

This document describes what functionality should be relagated to which libraries.

## JSON

For all JSON processing using nlohmann_json. This is provided via Conan

## CLI

For all command-line-interface (CLI) use CLI11. This is provided via Conan

## Logging

For all logging use spdlog. This is provided via Conan

## Subprocess

For all subprocessing, use cpp-subprocess. This is a single header provided under `src/external`.

## HTTP Requests

For all HTTP requests use cpr. This is provided via Conan


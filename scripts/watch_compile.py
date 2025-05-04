import asyncio
import pathlib
import subprocess
import sys
from typing import Final
from watchdog.observers import Observer
from colorama import Fore, Style, init as init_colorama
from watchdog.events import FileSystemEventHandler, FileModifiedEvent

init_colorama()

SHADER_EXTENSIONS: Final = {'.vert', '.frag', '.comp'}
DEBOUNCE_SECONDS: Final = 0.3


def should_compile(path: pathlib.Path) -> bool:
    return path.suffix in SHADER_EXTENSIONS


def compile_shader(shader_path: pathlib.Path, shader_output_dir: pathlib.Path, include_dir: pathlib.Path) -> None:
    shader_output_dir.mkdir(parents=True, exist_ok=True)
    output_spv = shader_output_dir / (shader_path.name + '.spv')

    result = subprocess.run(
        [
            'glslc',
            str(shader_path),
            '-o', str(output_spv),
            '-g',
            '-I', str(include_dir),
            '--target-env=vulkan1.4',
            '-x', 'glsl',
            '-Werror'
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )

    if result.returncode != 0:
        print(f'{Fore.RED}[Error] {shader_path.name}: {result.stderr.strip()}{Style.RESET_ALL}', file=sys.stderr)
    else:
        print(f'{Fore.GREEN}[Compiled] {shader_path.name}{Style.RESET_ALL}')

class DebouncedCompiler:
    def __init__(self, loop: asyncio.AbstractEventLoop, output_dir: pathlib.Path, include_dir: pathlib.Path):
        self._loop = loop
        self._output_dir = output_dir
        self._include_dir = include_dir
        self._tasks: dict[pathlib.Path, asyncio.TimerHandle] = {}

    def schedule(self, path: pathlib.Path) -> None:
        if path in self._tasks:
            self._tasks[path].cancel()

        handle = self._loop.call_later(DEBOUNCE_SECONDS, self._compile_sync, path)
        self._tasks[path] = handle

    def _compile_sync(self, path: pathlib.Path) -> None:
        self._tasks.pop(path, None)
        compile_shader(path, self._output_dir, self._include_dir)


class ShaderEventHandler(FileSystemEventHandler):
    def __init__(self, compiler: DebouncedCompiler, source_dir: pathlib.Path):
        self._compiler = compiler
        self._source_dir = source_dir.resolve()

    def on_modified(self, event: FileModifiedEvent) -> None:
        if not event.is_directory:
            path = pathlib.Path(event.src_path)
            if should_compile(path):
                self._compiler.schedule(path.resolve())


async def main(source: str, output: str) -> None:
    source_dir = pathlib.Path(source).resolve()
    output_dir = pathlib.Path(output).resolve()
    include_dir = source_dir / 'include'

    loop = asyncio.get_running_loop()
    compiler = DebouncedCompiler(loop, output_dir, include_dir)

    handler = ShaderEventHandler(compiler, source_dir)

    observer = Observer()
    observer.schedule(handler, str(source_dir), recursive=True)
    observer.start()

    print(f'Watching {source_dir} for shader changes...')

    try:
        while True:
            await asyncio.sleep(1)
    except KeyboardInterrupt:
        observer.stop()

    observer.join()


if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('--source', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()

    asyncio.run(main(args.source, args.output))

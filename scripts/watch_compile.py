import asyncio
import pathlib
import subprocess
import sys
import signal
from typing import Final
from watchdog.observers import Observer
from colorama import Fore, Style, init as init_colorama
from watchdog.events import FileSystemEventHandler, FileModifiedEvent

from compile_shaders import compile_all_shaders, compile_shader

init_colorama()

SHADER_EXTENSIONS: Final = {'.vert', '.frag', '.comp'}
DEBOUNCE_SECONDS: Final = 0.3


def should_compile(path: pathlib.Path) -> bool:
    return path.suffix in SHADER_EXTENSIONS


class DebouncedCompiler:
    def __init__(
        self,
        loop: asyncio.AbstractEventLoop,
        output_dir: pathlib.Path,
        include_dir: pathlib.Path,
        source_dir: pathlib.Path,
        optimize: bool,
    ):
        self._loop = loop
        self._output_dir = output_dir
        self._include_dir = include_dir
        self._source_dir = source_dir
        self._optimize = optimize
        self._tasks: dict[pathlib.Path, asyncio.TimerHandle] = {}

    def schedule(self, path: pathlib.Path) -> None:
        if path in self._tasks:
            self._tasks[path].cancel()
        handle = self._loop.call_later(
            DEBOUNCE_SECONDS, self._compile_sync, path)
        self._tasks[path] = handle

    def _compile_sync(self, path: pathlib.Path) -> None:
        self._tasks.pop(path, None)
        compile_shader(
            (path, self._output_dir, self._include_dir, self._optimize, False))

    def cancel_all(self) -> None:
        for handle in self._tasks.values():
            handle.cancel()
        self._tasks.clear()

    def pending_count(self) -> int:
        return len(self._tasks)

    def recompile_all(self) -> None:
        print(f'{Fore.YELLOW}Cleaning old .spv files...{Style.RESET_ALL}')
        for spv_file in self._output_dir.rglob('*.spv'):
            try:
                spv_file.unlink()
            except Exception as e:
                print(
                    f'{Fore.RED}[Error] Failed to delete {spv_file}: {e}{Style.RESET_ALL}')
        print(f'{Fore.YELLOW}Recompiling all shaders...{Style.RESET_ALL}')
        compile_all_shaders(self._source_dir, self._output_dir,
                            self._optimize, True)
        print(
            f'{Fore.CYAN}[Done] Full recompilation finished{Style.RESET_ALL}')


class ShaderEventHandler(FileSystemEventHandler):
    def __init__(self, compiler: DebouncedCompiler):
        self._compiler = compiler

    def on_modified(self, event: FileModifiedEvent) -> None:
        if not event.is_directory:
            path = pathlib.Path(event.src_path)
            if should_compile(path):
                self._compiler.schedule(path.resolve())


async def monitor_user_input(compiler: DebouncedCompiler, stop_event: asyncio.Event) -> None:
    while not stop_event.is_set():
        try:
            line = await asyncio.to_thread(sys.stdin.readline)
            if line.strip().lower() == 'r':
                compiler.recompile_all()
        except Exception:
            break


async def print_status(compiler: DebouncedCompiler, stop_event: asyncio.Event) -> None:
    while not stop_event.is_set():
        pending = compiler.pending_count()
        print(
            f'{Fore.BLUE}[Status] Pending shaders: {pending}{Style.RESET_ALL}', end='\r')
        await asyncio.sleep(1)


async def main(source: str, output: str, optimize: bool) -> None:
    source_dir = pathlib.Path(source).resolve()
    output_dir = pathlib.Path(output).resolve()
    include_dir = source_dir / 'include'

    loop = asyncio.get_running_loop()
    compiler = DebouncedCompiler(
        loop, output_dir, include_dir, source_dir, optimize)
    handler = ShaderEventHandler(compiler)

    observer = Observer()
    observer.schedule(handler, str(source_dir), recursive=True)
    observer.start()

    stop_event = asyncio.Event()

    def handle_exit(*_):
        stop_event.set()

    signal.signal(signal.SIGINT, handle_exit)
    signal.signal(signal.SIGTERM, handle_exit)

    print(f'Watching {source_dir} for shader changes...')
    print(
        f"Press {Fore.YELLOW}R{Style.RESET_ALL} + Enter to recompile all shaders.")

    await asyncio.gather(
        stop_event.wait(),
        monitor_user_input(compiler, stop_event),
        print_status(compiler, stop_event),
    )

    print('\nShutting down...')
    observer.stop()
    observer.join()
    compiler.cancel_all()


if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('--source', required=True)
    parser.add_argument('--output', required=True)
    parser.add_argument('--optimize', action='store_true', default=True)
    args = parser.parse_args()

    compile_all_shaders(pathlib.Path(args.source),
                        pathlib.Path(args.output), args.optimize, True)

    asyncio.run(main(args.source, args.output, args.optimize))

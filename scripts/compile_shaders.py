import os
import pathlib
import subprocess
import multiprocessing
import sys
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed

shader_extensions = ['.vert', '.frag', '.comp']


def find_shaders(shader_source_dir: pathlib.Path) -> list[pathlib.Path]:
    return [p for p in shader_source_dir.rglob('*') if p.suffix in shader_extensions]


def compile_shader(args: tuple[pathlib.Path, pathlib.Path, pathlib.Path, bool, bool]) -> tuple[pathlib.Path, bool, str]:
    shader_path, shader_binary_dir, include_dir, optimize, force = args
    shader_binary_dir.mkdir(parents=True, exist_ok=True)
    output_spv = shader_binary_dir / (shader_path.name + '.spv')

    if not force and output_spv.exists() and output_spv.stat().st_mtime >= shader_path.stat().st_mtime:
        return shader_path, False, ''

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
        print(
            f'[Error] {shader_path.name}: {result.stderr.strip()}', file=sys.stderr)
        return shader_path, True, result.stderr.strip()

    if optimize:
        opt_result = subprocess.run(
            ['spirv-opt', '-O', '--preserve-bindings', '--preserve-interface',
                str(output_spv), '-o', str(output_spv)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        if opt_result.returncode != 0:
            return shader_path, True, opt_result.stderr.strip()

    return shader_path, True, ''


def compile_all_shaders(shader_source_dir: pathlib.Path, shader_binary_dir: pathlib.Path, optimize: bool, force: bool) -> None:
    include_dir = shader_source_dir / 'include'
    shaders = find_shaders(shader_source_dir)
    total = len(shaders)

    if total == 0:
        return

    with ThreadPoolExecutor(max_workers=multiprocessing.cpu_count()) as executor:
        shader_args = [(shader, shader_binary_dir, include_dir,
                        optimize, force) for shader in shaders]
        future_to_shader = {executor.submit(
            compile_shader, arg): arg[0] for arg in shader_args}

        for i, future in enumerate(as_completed(future_to_shader), start=1):
            try:
                shader_path, was_compiled, error = future.result()

                if error:
                    print(
                        f'[Error] {shader_path.name}: {error}', file=sys.stderr)

                status = 'Compiled' if was_compiled else 'Up-to-date'
                print(f'[{i}/{total}] {status}: {shader_path.name}')
            except Exception as e:
                print(e)


def main() -> None:
    parser = argparse.ArgumentParser(
        description='Compile GLSL shaders to SPIR-V using glslc.')
    parser.add_argument('--source', required=True,
                        help='Path to the shader source directory')
    parser.add_argument('--output', required=True,
                        help='Path to the shader output directory')
    parser.add_argument('--optimize', action='store_true',
                        help='Run spirv-opt -O on compiled shaders')
    parser.add_argument('--force', action='store_true',
                        help='Force recompilation of all shaders regardless of timestamps')
    args = parser.parse_args()

    compile_all_shaders(pathlib.Path(args.source), pathlib.Path(
        args.output), args.optimize, args.force)


if __name__ == '__main__':
    main()

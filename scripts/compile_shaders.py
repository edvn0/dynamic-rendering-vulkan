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


def compile_shader(args: tuple[pathlib.Path, pathlib.Path, pathlib.Path]) -> tuple[pathlib.Path, bool, str]:
    shader_path, shader_binary_dir, include_dir = args
    shader_binary_dir.mkdir(parents=True, exist_ok=True)
    output_spv = shader_binary_dir / (shader_path.name + '.spv')

    if output_spv.exists() and output_spv.stat().st_mtime >= shader_path.stat().st_mtime:
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
        return shader_path, True, result.stderr.strip()

    return shader_path, True, ''


def compile_all_shaders(shader_source_dir: pathlib.Path, shader_binary_dir: pathlib.Path) -> None:
    include_dir = shader_source_dir / 'include'
    shaders = find_shaders(shader_source_dir)
    total = len(shaders)

    if total == 0:
        return

    compiled_count = 0

    with ThreadPoolExecutor(max_workers=multiprocessing.cpu_count()) as executor:
        shader_args = [(shader, shader_binary_dir, include_dir)
                       for shader in shaders]
        future_to_shader = {executor.submit(
            compile_shader, arg): arg[0] for arg in shader_args}

        for i, future in enumerate(as_completed(future_to_shader), start=1):
            shader_path, was_compiled, error = future.result()

            if error:
                print(f'[Error] {shader_path.name}: {error}', file=sys.stderr)
                sys.exit(1)

            status = 'Compiled' if was_compiled else 'Up-to-date'
            print(f'[{i}/{total}] {status}: {shader_path.name}')

            if was_compiled:
                compiled_count += 1


def main() -> None:
    parser = argparse.ArgumentParser(
        description='Compile GLSL shaders to SPIR-V using glslc.')
    parser.add_argument('--source', required=True,
                        help='Path to the shader source directory')
    parser.add_argument('--output', required=True,
                        help='Path to the shader output directory')
    args = parser.parse_args()

    compile_all_shaders(pathlib.Path(args.source), pathlib.Path(args.output))


if __name__ == '__main__':
    main()

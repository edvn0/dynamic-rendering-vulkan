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
            'glslangValidator',
            '-V', str(shader_path),
            '-o', str(output_spv),
            '-gVS',
            f'-I{include_dir}',
            '-t'
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )

    if result.returncode != 0:
        return shader_path, True, result.stderr.strip()

    return shader_path, True, ''

def main() -> None:
    parser = argparse.ArgumentParser(description='Compile GLSL shaders to SPIR-V.')
    parser.add_argument('--source', required=True, help='Path to the shader source directory')
    parser.add_argument('--output', required=True, help='Path to the shader output directory')
    args = parser.parse_args()

    shader_source_dir = pathlib.Path(args.source)
    shader_binary_dir = pathlib.Path(args.output)
    include_dir = shader_source_dir / 'include'

    shaders = find_shaders(shader_source_dir)
    total = len(shaders)

    if total == 0:
        print('No shaders found.')
        return

    print(f'Found {total} shader(s). Starting compilation with {multiprocessing.cpu_count()} threads...\n')

    compiled_count = 0

    with ThreadPoolExecutor(max_workers=multiprocessing.cpu_count()) as executor:
        shader_args = [(shader, shader_binary_dir, include_dir) for shader in shaders]
        future_to_shader = {executor.submit(compile_shader, arg): arg[0] for arg in shader_args}

        for i, future in enumerate(as_completed(future_to_shader), start=1):
            shader_path, was_compiled, error = future.result()

            if error:
                print(f'[Error] {shader_path.name}: {error}', file=sys.stderr)
                sys.exit(1)

            status = 'Compiled' if was_compiled else 'Up-to-date'
            print(f'[{i}/{total}] {status}: {shader_path.name}')

            if was_compiled:
                compiled_count += 1

    print(f'\nShader compilation finished: {compiled_count}/{total} recompiled.')

if __name__ == '__main__':
    main()


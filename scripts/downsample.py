#!/usr/bin/env python3
"""
Texture Downsampler - Converts 4K textures to 1K resolution
"""

import argparse
import sys
from pathlib import Path
from PIL import Image
from multiprocessing import Pool, cpu_count
from tqdm import tqdm


class ProgressTracker:
    """Thread-safe progress tracker for multiprocessing."""

    def __init__(self, total_files, show_progress=True):
        self.total_files = total_files
        self.show_progress = show_progress
        self.successful_count = 0
        self.failed_count = 0
        self.messages = []

        if self.show_progress:
            self.pbar = tqdm(
                total=total_files,
                desc="Processing textures",
                unit="files",
                ncols=100,
                bar_format='{l_bar}{bar}| {n_fmt}/{total_fmt} [{elapsed}<{remaining}, {rate_fmt}]'
            )

    def update_progress(self, result):
        """Callback function called when each process completes."""
        success, message = result

        if success:
            self.successful_count += 1
        else:
            self.failed_count += 1
            self.messages.append((False, message))

        if self.show_progress:
            # Update progress bar description with current stats
            desc = f"✓{self.successful_count} ✗{self.failed_count}"
            self.pbar.set_description(desc)
            self.pbar.update(1)

    def close(self):
        """Close the progress bar and print summary."""
        if self.show_progress:
            self.pbar.close()

        # Print all messages after progress bar is done
        for success, message in self.messages:
            if success:
                print(message)
            else:
                print(message, file=sys.stderr)

    def get_stats(self):
        """Get final statistics."""
        return self.successful_count, self.failed_count


def downsample_image(input_path, output_path, target_size=(1024, 1024)):
    """
    Downsample an image to target size using high-quality resampling.

    Args:
        input_path (Path): Path to input image
        output_path (Path): Path to output image
        target_size (tuple): Target dimensions as (width, height)

    Returns:
        tuple: (success: bool, message: str)
    """
    try:
        with Image.open(input_path) as img:
            # Use LANCZOS for high-quality downsampling
            downsampled = img.resize(target_size, Image.Resampling.LANCZOS)

            # Preserve original format and quality
            save_kwargs = {}
            if img.format == 'JPEG':
                save_kwargs['quality'] = 95
                save_kwargs['optimize'] = True
            elif img.format == 'PNG':
                save_kwargs['optimize'] = True

            downsampled.save(output_path, **save_kwargs)
            return True, f"Processed: {input_path.name} -> {output_path.name}"

    except Exception as e:
        return False, f"Error processing {input_path.name}: {e}"


def process_single_texture(args):
    """
    Process a single texture file. Used for multiprocessing.

    Args:
        args (tuple): (input_file_path, output_file_path, target_size)

    Returns:
        tuple: (success: bool, message: str)
    """
    input_file, output_file, target_size = args
    return downsample_image(input_file, output_file, target_size)


def process_textures(input_folder, output_folder, extensions, target_size=(1024, 1024), num_processes=None, show_progress=True):
    """
    Process all texture files in the input folder using multiprocessing with progress tracking.

    Args:
        input_folder (Path): Input directory path
        output_folder (Path): Output directory path
        extensions (list): List of file extensions to process
        target_size (tuple): Target dimensions as (width, height)
        num_processes (int): Number of processes to use (None for auto)
        show_progress (bool): Whether to show progress bar
    """
    input_path = Path(input_folder)
    output_path = Path(output_folder)

    # Validate input folder
    if not input_path.exists():
        print(
            f"Error: Input folder '{input_folder}' does not exist", file=sys.stderr)
        return False

    if not input_path.is_dir():
        print(f"Error: '{input_folder}' is not a directory", file=sys.stderr)
        return False

    # Create output folder if it doesn't exist
    output_path.mkdir(parents=True, exist_ok=True)

    # Normalize extensions (add dots and convert to lowercase)
    normalized_extensions = []
    for ext in extensions:
        if not ext.startswith('.'):
            ext = '.' + ext
        normalized_extensions.append(ext.lower())

    # Find all matching files
    texture_files = []
    for ext in normalized_extensions:
        texture_files.extend(input_path.glob(f'*{ext}'))
        texture_files.extend(input_path.glob(f'*{ext.upper()}'))

    if not texture_files:
        print(f"No texture files found with extensions: {extensions}")
        return True

    print(f"Found {len(texture_files)} texture files to process")

    # Determine number of processes
    if num_processes is None:
        num_processes = min(cpu_count(), len(texture_files))

    print(f"Using {num_processes} processes")

    # Prepare arguments for multiprocessing
    process_args = []
    for texture_file in texture_files:
        output_file = output_path / texture_file.name
        process_args.append((texture_file, output_file, target_size))

    # Initialize progress tracker
    progress_tracker = ProgressTracker(len(texture_files), show_progress)

    # Process files using multiprocessing with progress tracking
    try:
        with Pool(processes=num_processes) as pool:
            # Use map_async with callback for real-time progress updates
            result = pool.map_async(
                process_single_texture,
                process_args,
                callback=None,  # We'll handle this per-item
                error_callback=lambda e: print(
                    f"Pool error: {e}", file=sys.stderr)
            )

            # Alternative approach: use imap for real-time updates
            results = []
            for i, res in enumerate(pool.imap(process_single_texture, process_args)):
                progress_tracker.update_progress(res)
                results.append(res)

            pool.close()
            pool.join()

    except KeyboardInterrupt:
        print("\nInterrupted by user")
        pool.terminate()
        pool.join()
        return False
    except Exception as e:
        print(f"Error during processing: {e}", file=sys.stderr)
        return False
    finally:
        progress_tracker.close()

    # Get final statistics
    successful_count, failed_count = progress_tracker.get_stats()

    print(f"\nCompleted! Successfully processed {successful_count} files")
    if failed_count > 0:
        print(f"Failed to process {failed_count} files")

    return True


def main():
    """Main function with argument parsing."""
    parser = argparse.ArgumentParser(
        description="Downsample 4K textures to 1K resolution",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s input_textures/ output_textures/
  %(prog)s input/ output/ --extensions png jpg jpeg tga
  %(prog)s ~/textures/4k/ ~/textures/1k/ -e png -j 8
  %(prog)s input/ output/ --jobs 4 --target-width 2048
  %(prog)s input/ output/ --no-progress  # Disable progress bar
        """
    )

    parser.add_argument(
        'input_folder',
        help='Input folder containing 4K textures'
    )

    parser.add_argument(
        'output_folder',
        help='Output folder for 1K textures'
    )

    parser.add_argument(
        '-e', '--extensions',
        nargs='+',
        default=['png', 'jpg'],
        help='File extensions to process (default: png jpg)'
    )

    parser.add_argument(
        '--target-width',
        type=int,
        default=1024,
        help='Target width in pixels (default: 1024)'
    )

    parser.add_argument(
        '--target-height',
        type=int,
        default=1024,
        help='Target height in pixels (default: 1024)'
    )

    parser.add_argument(
        '-j', '--jobs',
        type=int,
        default=None,
        help='Number of parallel processes (default: auto-detect based on CPU cores)'
    )

    parser.add_argument(
        '--no-progress',
        action='store_true',
        help='Disable progress bar'
    )

    args = parser.parse_args()

    # Update target size if custom dimensions provided
    target_size = (args.target_width, args.target_height)

    success = process_textures(
        args.input_folder,
        args.output_folder,
        args.extensions,
        target_size,
        args.jobs,
        not args.no_progress
    )

    if not success:
        sys.exit(1)


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""
offline-md.py - Download remote images from markdown files and make them local

A CLI tool that processes markdown files, downloads remote images, and updates
the markdown to reference local copies in an 'attachments' folder.

Usage:
    uv run offline-md.py path/to/file.md
    python offline-md.py path/to/file.md

Requirements: No external dependencies - uses only Python standard library.
"""
# /// script
# requires-python = ">=3.7"
# ///

import argparse
import os
import re
import sys
import urllib.parse
import urllib.request
from pathlib import Path
from typing import List, Tuple


def extract_image_urls(content: str) -> List[Tuple[str, str]]:
    """
    Extract image URLs from markdown content.
    
    Returns list of tuples: (original_markdown_syntax, image_url)
    """
    # Pattern to match markdown images: ![alt](url) and HTML img tags
    patterns = [
        # Markdown syntax: ![alt](url)
        r'!\[([^\]]*)\]\(([^)]+)\)',
        # HTML img tags: <img src="url" ...>
        r'<img[^>]+src=["\']([^"\']+)["\'][^>]*>',
    ]
    
    matches = []
    for pattern in patterns:
        for match in re.finditer(pattern, content):
            if pattern.startswith('!'):
                # Markdown syntax
                full_match = match.group(0)
                url = match.group(2)
            else:
                # HTML img tag
                full_match = match.group(0)
                url = match.group(1)
            
            # Only process remote URLs (http/https)
            if url.startswith(('http://', 'https://')):
                matches.append((full_match, url))
    
    return matches


def sanitize_filename(url: str) -> str:
    """
    Create a safe filename from a URL.
    """
    # Parse the URL to get the filename
    parsed = urllib.parse.urlparse(url)
    filename = os.path.basename(parsed.path)
    
    # If no filename in path, create one from the URL
    if not filename or '.' not in filename:
        # Use the last part of the path or create a generic name
        path_parts = [p for p in parsed.path.split('/') if p]
        if path_parts:
            filename = path_parts[-1]
        else:
            filename = 'image'
        
        # Try to guess extension from URL or use .jpg as default
        if not '.' in filename:
            filename += '.jpg'
    
    # Sanitize the filename
    filename = re.sub(r'[<>:"/\\|?*]', '_', filename)
    filename = filename.strip('.')
    
    # Ensure it's not empty
    if not filename:
        filename = 'image.jpg'
    
    return filename


def download_image(url: str, filepath: Path) -> bool:
    """
    Download an image from URL to filepath.
    
    Returns True if successful, False otherwise.
    """
    try:
        # Create a request with a user agent to avoid blocking
        req = urllib.request.Request(
            url,
            headers={
                'User-Agent': 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36'
            }
        )
        
        with urllib.request.urlopen(req, timeout=30) as response:
            if response.status == 200:
                filepath.parent.mkdir(parents=True, exist_ok=True)
                with open(filepath, 'wb') as f:
                    f.write(response.read())
                print(f"✓ Downloaded: {filepath.name}")
                return True
            else:
                print(f"✗ HTTP {response.status} for: {url}")
                return False
                
    except Exception as e:
        print(f"✗ Failed to download {url}: {e}")
        return False


def process_markdown_file(md_path: Path) -> bool:
    """
    Process a markdown file, downloading images and updating references.
    
    Returns True if any changes were made.
    """
    if not md_path.exists():
        print(f"Error: File {md_path} does not exist")
        return False
    
    if not md_path.suffix.lower() == '.md':
        print(f"Warning: {md_path} doesn't have .md extension")
    
    # Read the markdown file
    try:
        with open(md_path, 'r', encoding='utf-8') as f:
            content = f.read()
    except Exception as e:
        print(f"Error reading {md_path}: {e}")
        return False
    
    # Extract image URLs
    image_matches = extract_image_urls(content)
    
    if not image_matches:
        print("No remote images found in the markdown file")
        return False
    
    print(f"Found {len(image_matches)} remote image(s)")
    
    # Create attachments directory
    attachments_dir = md_path.parent / 'attachments'
    attachments_dir.mkdir(exist_ok=True)
    
    # Download images and update content
    updated_content = content
    changes_made = False
    
    for original_syntax, url in image_matches:
        filename = sanitize_filename(url)
        
        # Handle duplicate filenames
        counter = 1
        base_name, ext = os.path.splitext(filename)
        while (attachments_dir / filename).exists():
            filename = f"{base_name}_{counter}{ext}"
            counter += 1
        
        filepath = attachments_dir / filename
        
        print(f"Downloading: {url}")
        if download_image(url, filepath):
            # Update the markdown content
            relative_path = f"attachments/{filename}"
            
            # Replace the original syntax with local path
            if original_syntax.startswith('!['):
                # Markdown syntax: preserve alt text
                alt_match = re.match(r'!\[([^\]]*)\]', original_syntax)
                alt_text = alt_match.group(1) if alt_match else ''
                new_syntax = f"![{alt_text}]({relative_path})"
            else:
                # HTML img tag: replace src attribute
                new_syntax = re.sub(
                    r'src=["\'][^"\']+["\']',
                    f'src="{relative_path}"',
                    original_syntax
                )
            
            updated_content = updated_content.replace(original_syntax, new_syntax)
            changes_made = True
        else:
            print(f"Skipping update for failed download: {url}")
    
    # Write updated content back to file
    if changes_made:
        try:
            # Create backup
            backup_path = md_path.with_suffix(md_path.suffix + '.backup')
            md_path.rename(backup_path)
            print(f"Created backup: {backup_path}")
            
            # Write updated file
            with open(md_path, 'w', encoding='utf-8') as f:
                f.write(updated_content)
            
            print(f"✓ Updated markdown file: {md_path}")
            return True
            
        except Exception as e:
            print(f"Error writing updated file: {e}")
            # Restore backup if it exists
            if backup_path.exists():
                backup_path.rename(md_path)
                print("Restored original file from backup")
            return False
    
    return False


def main():
    """Main CLI entry point."""
    parser = argparse.ArgumentParser(
        description="Download remote images from markdown files and make them local",
        epilog="Example: uv run offline-md.py docs/my-file.md"
    )
    parser.add_argument(
        'markdown_file',
        help='Path to the markdown file to process'
    )
    parser.add_argument(
        '--dry-run',
        action='store_true',
        help='Show what would be downloaded without actually downloading'
    )
    
    args = parser.parse_args()
    
    md_path = Path(args.markdown_file)
    
    if args.dry_run:
        print("DRY RUN MODE - No files will be downloaded or modified")
        if not md_path.exists():
            print(f"Error: File {md_path} does not exist")
            sys.exit(1)
        
        try:
            with open(md_path, 'r', encoding='utf-8') as f:
                content = f.read()
        except Exception as e:
            print(f"Error reading {md_path}: {e}")
            sys.exit(1)
        
        image_matches = extract_image_urls(content)
        if image_matches:
            print(f"Would download {len(image_matches)} image(s):")
            for original_syntax, url in image_matches:
                filename = sanitize_filename(url)
                print(f"  {url} -> attachments/{filename}")
        else:
            print("No remote images found")
        return
    
    success = process_markdown_file(md_path)
    
    if success:
        print("\n✓ Successfully processed markdown file!")
        print(f"Images saved to: {md_path.parent / 'attachments'}")
    else:
        print("\n✗ No changes made or errors occurred")
        sys.exit(1)


if __name__ == '__main__':
    main()

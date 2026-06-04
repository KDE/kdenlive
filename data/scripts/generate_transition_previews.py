#!/usr/bin/env python3

"""
SPDX-FileCopyrightText: 2025 KDE Community
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

Script to generate animated GIF previews for Kdenlive transitions.
This script queries available MLT transitions and creates preview GIFs
showing the transition effect between two colored clips.
"""

import subprocess
import json
import os
import sys
import argparse
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import logging
import urllib.parse

# Set up logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

class TransitionPreviewGenerator:
    def __init__(self, output_dir, xml_dir=None, luma_path=None, image_path1=None, image_path2=None, size=(320, 180), duration=15, mix_duration=30, file_format="webp"):
        """
        Initialize the preview generator
        
        Args:
            output_dir (str): Directory to store generated previews
            xml_dir (str): Path to the transitions xml files
            image_path1 (str): Path to first image (optional)
            image_path2 (str): Path to second image (optional)
            size (tuple): Width and height of preview (default: 320x180)
            duration (int): Duration of each clip in frames
            mix_duration (int): Duration of transition in frames
            luma_path (str): Path to luma files
            file_format (str) : File format (extension) for rendering
        """
        self.output_dir = Path(output_dir)
        self.xml_dir = xml_dir
        self.width, self.height = size
        self.duration = duration
        self.mix_duration = mix_duration
        self.image_path1 = image_path1
        self.image_path2 = image_path2
        self.luma_path = luma_path
        self.file_format = file_format
        
        # Create output directory
        self.output_dir.mkdir(parents=True, exist_ok=True)
        
        # Standard MLT profile for preview generation
        self.profile = "qcif_15" # Using 15fps for smaller file sizes
    
    def get_available_lumas(self):
        """Query available lumas in selected folder"""
        try:
            if not self.luma_path or self.luma_path == None:
                logger.error(f"Empty luma path...")
                return []
            lumas = []
            logger.error(f"INIT LUMA SEARCH ON: {self.luma_path}")
            luma_folders = self.luma_path.split()
            for l in luma_folders:
                directory_path = Path(urllib.parse.unquote(l))
                logger.error(f"Checking luma path: {l} / {directory_path}")
                for path_object in directory_path.rglob("*.png"):
                    logger.error(f"APPENDING PNG luma: {path_object}")
                    lumas.append(str(path_object))
                for path_object in directory_path.rglob("*.pgm"):
                    logger.error(f"APPENDING luma: {path_object}")
                    lumas.append(str(path_object))
            # Add MLT internal lumas
            for k in range(1,23,1):
                lumas.append(f"luma{k:02}.pgm")
            logger.error(f"FOUND lumas: {lumas}")
            return lumas
        except Exception as e:
            logger.error(f"Unexpected error querying lumas: {e}")
            return []

    def get_available_transitions(self):
        transitionsXml = []
        if self.xml_dir is None:
            xml_folders = ''
        else:
            xml_folders =  self.xml_dir.split()

        for l in xml_folders:
            directory_path = Path(urllib.parse.unquote(l))
            logger.error(f"Checking transitions path: {directory_path}")
            for path_object in directory_path.rglob("*.xml"):
                logger.error(f"APPENDING transition: {path_object}")
                transitionsXml.append(str(path_object))
        return transitionsXml

    def create_luma_preview(self, luma_id):
        """
        Create a preview webp for a specific luma

        Args:
        luma_id (str): The MLT luma file
        """
        file_path = Path(luma_id).stem
        logger.error(f"Processing luma: {luma_id}")
        output_path = self.output_dir / f"{file_path}.{self.file_format}"

        # Skip if preview already exists
        if output_path.exists():
            logger.info(f"Preview for {luma_id} already exists, skipping...")
            return

        try:
            # Basic command structure
            command = [
                'melt',
                '-profile', self.profile,
            ]

            # First clip
            if self.image_path1 is None:
                command.extend(['color:0x253d4bff', f'out={self.duration}', '-attach', 'qtext:A', 'size=100', 'fgcolour=white', 'bgcolour=transparent', 'valign=middle', 'halign=center', 'weight=800'])
            else:
                command.extend([self.image_path1, f'out={self.duration}'])

            # Second clip
            if self.image_path2 is None:
                command.extend(['color:0x91cdfaff', f'out={self.duration}', '-attach', 'qtext:B', 'size=100', 'fgcolour=0x505050ff', 'bgcolour=transparent', 'valign=middle', 'halign=center', 'weight=800'])
            else:
                command.extend([self.image_path2, f'out={self.duration}'])

            # Apply transition
            command.extend(['-mix', str(self.mix_duration), '-mixer', 'luma', f'resource={luma_id}'])

            # Add output settings
            command.extend([
                '-consumer', f'avformat:{output_path}',
                f'width={self.width}',
                f'height={self.height}',
                'fps=10'
            ])
            if str(output_path).endswith("webp"):
                # webp format
                command.extend([
                    'quality=50',
                    'loop=0'
                ])

            logger.info(f"Generating preview for {luma_id}...")
            logger.info(f"Command: {' '.join(command)}")
            result = subprocess.run(command, capture_output=True, text=True)

            if result.returncode == 0:
                logger.info(f"Successfully generated preview for {luma_id}")
            else:
                logger.error(f"Failed to generate preview for {luma_id}: {result.stderr}")

        except Exception as e:
            logger.error(f"Error generating preview for {luma_id}: {e}")

    def create_transition_preview(self, transition_path):
        """
        Create a preview GIF for a specific transition
        
        Args:
            transition_id (str): The MLT transition identifier
        """
        from xml.dom import minidom

        xmldoc = minidom.parse(transition_path)
        top_element = xmldoc.documentElement
        if not top_element.hasAttribute('tag'):
            logger.info(f"Xml for {transition_path} has no tag...")
            return
        mlt_tag = top_element.getAttribute('tag')
        transition_params = []
        if top_element.hasAttribute('id'):
            transition_id = top_element.getAttribute('id')
        else:
            transition_id = mlt_tag
        try:
            preview_tag = xmldoc.getElementsByTagName("preview")[0]
            params = preview_tag.getElementsByTagName("parameter")
            for p in params:
                p_name = p.getAttribute('name')
                p_value = p.firstChild.nodeValue
                transition_params.append(f'{p_name}="{p_value}"')
                logger.error(f"FOUND param for transition {mlt_tag} = {p_name}={p_value}")
        except:
            logger.error(f"No preview param for transition {mlt_tag}")

        output_path = self.output_dir / f"{transition_id}.{self.file_format}"
        
        # Skip if preview already exists
        if output_path.exists():
            logger.info(f"Preview {output_path} for {transition_id} already exists, skipping...")
            return
        
        try:
            # Basic command structure
            command = [
                'melt',
                '-profile', self.profile,
            ]
            
            # First clip
            if self.image_path1 is None:
                command.extend(['color:0x253d4bff', f'out={self.duration}', '-attach', 'qtext:A', 'size=100', 'fgcolour=white', 'bgcolour=transparent', 'valign=middle', 'halign=center', 'weight=800'])
            else:
                command.extend([self.image_path1, f'out={self.duration}'])
            
            # Second clip
            if self.image_path2 is None:
                command.extend(['color:0x91cdfaff', f'out={self.duration}', '-attach', 'qtext:B', 'size=100', 'fgcolour=0x505050ff', 'bgcolour=transparent', 'valign=middle', 'halign=center', 'weight=800'])
            else:
                command.extend([self.image_path2, f'out={self.duration}'])
            
            # Apply transition
            command.extend(['-mix', str(self.mix_duration), '-mixer', mlt_tag])
            
            # Add transition-specific parameters if available
            for param in transition_params:
                command.append(param)
            
            # Add output settings
            command.extend([
                '-consumer', f'avformat:{output_path}',
                f'width={self.width}',
                f'height={self.height}',
                'fps=10',
                'quality=50',
                'loop=0'
            ])
            
            logger.info(f"Generating preview for {transition_id}...")
            logger.info(f"Command: {' '.join(command)}")
            result = subprocess.run(command, capture_output=True, text=True)
            
            if result.returncode == 0:
                logger.info(f"Successfully generated preview for {transition_id}")
            else:
                logger.error(f"Failed to generate preview for {transition_id}: {result.stderr}")
                
        except Exception as e:
            logger.error(f"Error generating preview for {transition_id}: {e}")

    def generate_all_previews(self, max_workers=4):
        """
        Generate previews for all available transitions
        
        Args:
            max_workers (int): Number of parallel preview generations
        """
        transitions = self.get_available_transitions()
        lumas = self.get_available_lumas()
        
        if not transitions:
            logger.error("No transitions found. Make sure MLT is properly installed.")
        
        logger.info(f"Starting preview generation for {len(transitions)} transitions...")
        
        with ThreadPoolExecutor(max_workers=max_workers) as executor:
            list(executor.map(self.create_transition_preview, transitions))

        with ThreadPoolExecutor(max_workers=max_workers) as executor:
                list(executor.map(self.create_luma_preview, lumas))
        
        logger.info("Preview generation completed!")

def main():
    parser = argparse.ArgumentParser(description='Generate transition previews for Kdenlive')
    
    parser.add_argument(
        '--output-dir',
        default='data/transitions/previews',
        help='Directory to store generated previews'
    )
    
    parser.add_argument(
        '--xml-dir',
        help='Path to the transitions xml folder'
    )

    parser.add_argument(
        '--luma-path',
        help='Path to luma files'
    )
    
    parser.add_argument(
        '--image1',
        help='Path to first image for transition preview'
    )
    
    parser.add_argument(
        '--image2',
        help='Path to second image for transition preview'
    )
    
    parser.add_argument(
        '--width',
        type=int,
        default=320,
        help='Preview width'
    )
    
    parser.add_argument(
        '--height',
        type=int,
        default=180,
        help='Preview height'
    )
    
    parser.add_argument(
        '--duration',
        type=int,
        default=10,
        help='Duration of each clip in frames'
    )
    parser.add_argument(
        '--file-format',
        default="webp",
        help='Output format for renders'
    )
    
    parser.add_argument(
        '--mix-duration',
        type=int,
        default=20,
        help='Duration of transition in frames'
    )
    
    parser.add_argument(
        '--workers',
        type=int,
        default=4,
        help='Number of parallel preview generations'
    )
    
    args = parser.parse_args()
    
    generator = TransitionPreviewGenerator(
        args.output_dir,
        xml_dir=args.xml_dir,
        image_path1=args.image1,
        image_path2=args.image2,
        size=(args.width, args.height),
        duration=args.duration,
        mix_duration=args.mix_duration,
        luma_path=args.luma_path,
        file_format=args.file_format

    )
    
    generator.generate_all_previews(max_workers=args.workers)

if __name__ == '__main__':
    main() 

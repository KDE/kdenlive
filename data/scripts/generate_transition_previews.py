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

# Set up logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

class TransitionPreviewGenerator:
    def __init__(self, output_dir, param_file=None, luma_path=None, image_path1=None, image_path2=None, size=(320, 180), duration=20, mix_duration=40):
        """
        Initialize the preview generator
        
        Args:
            output_dir (str): Directory to store generated previews
            param_file (str): Path to the transition parameters file
            image_path1 (str): Path to first image (optional)
            image_path2 (str): Path to second image (optional)
            size (tuple): Width and height of preview (default: 320x180)
            duration (int): Duration of each clip in frames
            mix_duration (int): Duration of transition in frames
            luma_path (str): Path to luma files
        """
        self.output_dir = Path(output_dir)
        self.width, self.height = size
        self.duration = duration
        self.mix_duration = mix_duration
        self.transition_params = {}
        self.image_path1 = image_path1
        self.image_path2 = image_path2
        self.luma_path = luma_path
        
        # Create output directory
        self.output_dir.mkdir(parents=True, exist_ok=True)
        
        # Load transition parameters if file provided
        if param_file:
            self.load_transition_parameters(param_file)
        
        # Standard MLT profile for preview generation
        self.profile = "qcif_15" # Using 15fps for smaller file sizes
    
    def load_transition_parameters(self, param_file):
        """
        Load transition parameters from a file
        
        Args:
            param_file (str): Path to the parameter file
        """
        try:
            param_file_path = Path(param_file)
            if not param_file_path.exists():
                logger.warning(f"Parameters file {param_file} not found")
                return
            
            logger.info(f"Loading transition parameters from {param_file}")
            with open(param_file_path, 'r') as f:
                for line in f:
                    line = line.strip()
                    if not line or line.startswith('#'):
                        continue
                    
                    try:
                        transition_id, params = line.split('=', 1)
                        self.transition_params[transition_id.strip()] = params.strip()
                    except ValueError:
                        logger.warning(f"Invalid parameter line: {line}")
            
            logger.info(f"Loaded parameters for {len(self.transition_params)} transitions")
        except Exception as e:
            logger.error(f"Error loading transition parameters: {e}")

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
                directory_path = Path(l)
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
        """Query MLT for available transitions"""
        try:
            result = subprocess.run(
                ['melt', '-query', 'transitions'],
                capture_output=True,
                text=True
            )
            
            # Split output into lines
            lines = result.stdout.split('\n')
            
            # Find where the transitions list starts
            for i, line in enumerate(lines):
                if line.strip() == 'transitions:':
                    transitions_start = i + 1
                    break
            else:
                logger.error("Could not find transitions list in MLT output")
                return []
            
            # Parse transitions
            transitions = []
            for line in lines[transitions_start:]:
                line = line.strip()
                if line.startswith('- '):
                    # Extract transition name
                    transition_id = line[2:].strip()
                    transitions.append(transition_id)
                elif line == '...' or not line:
                    # End of transitions list
                    break
            
            logger.info(f"Found {len(transitions)} transitions: {', '.join(transitions)}")
            return transitions
            
        except subprocess.CalledProcessError as e:
            logger.error(f"Failed to query transitions: {e}")
            return []
        except Exception as e:
            logger.error(f"Unexpected error querying transitions: {e}")
            return []

    def create_luma_preview(self, luma_id):
        """
        Create a preview GIF for a specific luma

        Args:
        luma_id (str): The MLT luma file
        """
        file_path = Path(luma_id).stem
        logger.error(f"Processing luma: {luma_id}")
        output_path = self.output_dir / f"{file_path}.gif"

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
                command.extend(['color:red', f'out={self.duration}', '-attach', 'qtext:A', 'size=100', 'fgcolour=white', 'bgcolour=transparent', 'valign=middle', 'halign=center', 'weight=800'])
            else:
                command.extend([self.image_path1, f'out={self.duration}'])

            # Second clip
            if self.image_path2 is None:
                command.extend(['color:blue', f'out={self.duration}', '-attach', 'qtext:B', 'size=100', 'fgcolour=white', 'bgcolour=transparent', 'valign=middle', 'halign=center', 'weight=800'])
            else:
                command.extend([self.image_path2, f'out={self.duration}'])

            # Apply transition
            command.extend(['-mix', str(self.mix_duration), '-mixer', 'luma', f'resource={luma_id}'])

            # Add output settings
            command.extend([
                '-consumer', f'avformat:{output_path}',
                f'width={self.width}',
                f'height={self.height}',
                'fps=15'
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

    def create_transition_preview(self, transition_id):
        """
        Create a preview GIF for a specific transition
        
        Args:
            transition_id (str): The MLT transition identifier
        """
        output_path = self.output_dir / f"{transition_id}.gif"
        
        # Skip if preview already exists
        if output_path.exists():
            logger.info(f"Preview for {transition_id} already exists, skipping...")
            return
        
        try:
            # Basic command structure
            command = [
                'melt',
                '-profile', self.profile,
            ]
            
            # First clip
            if self.image_path1 is None:
                command.extend(['color:red', f'out={self.duration}', '-attach', 'qtext:A', 'size=100', 'fgcolour=white', 'bgcolour=transparent', 'valign=middle', 'halign=center', 'weight=800'])
            else:
                command.extend([self.image_path1, f'out={self.duration}'])
            
            # Second clip
            if self.image_path2 is None:
                command.extend(['color:blue', f'out={self.duration}', '-attach', 'qtext:B', 'size=100', 'fgcolour=white', 'bgcolour=transparent', 'valign=middle', 'halign=center', 'weight=800'])
            else:
                command.extend([self.image_path2, f'out={self.duration}'])
            
            # Apply transition
            command.extend(['-mix', str(self.mix_duration), '-mixer', transition_id])
            
            # Add transition-specific parameters if available
            if transition_id in self.transition_params:
                command.append(self.transition_params[transition_id])
                logger.info(f"Using custom parameters for {transition_id}: {self.transition_params[transition_id]}")
            
            # Add output settings
            command.extend([
                '-consumer', f'avformat:{output_path}',
                f'width={self.width}',
                f'height={self.height}',
                'fps=15'
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
            return
        
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
        '--param-file',
        help='Path to the transition parameters file'
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
        default=20,
        help='Duration of each clip in frames'
    )
    
    parser.add_argument(
        '--mix-duration',
        type=int,
        default=40,
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
        param_file=args.param_file,
        image_path1=args.image1,
        image_path2=args.image2,
        size=(args.width, args.height),
        duration=args.duration,
        mix_duration=args.mix_duration,
        luma_path=args.luma_path
    )
    
    generator.generate_all_previews(max_workers=args.workers)

if __name__ == '__main__':
    main() 

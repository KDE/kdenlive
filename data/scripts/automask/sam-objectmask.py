#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

import os
# if using Apple MPS, fall back to CPU for unsupported ops
# os.environ["PYTORCH_ENABLE_MPS_FALLBACK"] = "1"
import numpy as np
import torch
import cv2
import sys
import argparse
from PIL import Image

def process_csv(csv_string, resize):
    # Convert the CSV string back to a NumPy array
    resulting_array = {}
    vals_list = csv_string.split(';')
    for vals in vals_list:
        frame,csv_data = vals.split("=")
        np_array = np.fromstring(csv_data, dtype=int, sep=',')

        # Reshape the array if necessary (e.g., if it was a 2D array)
        if resize > 1:
            cols = int((np.shape(np_array)[0])/resize)
            print("Resize Array, COLS:")
            print(cols)
            np_array = np_array.reshape(cols, resize)
        resulting_array[int(frame)] = np_array

    print("NumPy Array:")
    print(resulting_array)
    return resulting_array

if __name__ == "__main__":
    parser = argparse.ArgumentParser("SAM Object Mask Creator")
    parser.add_argument("-P", "--point_coordinates", help="Points coordinates with frame, like '0=200,250,300,255;100=10,50' for 2 points at frame 0 and one at frame 100")
    parser.add_argument("-F", "--preview_frame", help="The frame index for preview", default=-1)
    parser.add_argument("-L", "--labels", help="Points labels, 1 for include, 0 for exclude, like '0=1,0;100=1' for frame 0 and 100")
    parser.add_argument("-B", "--box_coordinates", help="Box coordinates with frame, like '0=10,20,150,255'")
    parser.add_argument("-I", "--inputFolder", help="folder where input jpg files are stored", default="/tmp/src-frames")
    parser.add_argument("-O", "--output", help="path for rendered jpg image for preview of folder for rendering", default="/tmp/preview.png")
    parser.add_argument("-M", "--model", help="path for the model")
    parser.add_argument("-C", "--config", help="config for the model")
    args = parser.parse_args()
    if (args.point_coordinates == None or args.labels == None) and args.box_coordinates == None:
        config = vars(args)
        print(config)
        sys.exit()

    box = {}
    points = {}
    labels = {}
    if args.point_coordinates != None:
        points = process_csv(args.point_coordinates, 2)
        labels = process_csv(args.labels, 1)
    if args.box_coordinates != None:
        box = process_csv(args.box_coordinates, 4)
    preview_frame = int(args.preview_frame)
    output_frame = args.output
    inputFolder = args.inputFolder
    modelFile = args.model
    configFile = args.config
    borders = 0

# select the device for computation
if torch.cuda.is_available():
    device = torch.device("cuda")
elif torch.backends.mps.is_available():
    device = torch.device("mps")
else:
    device = torch.device("cpu")
print(f"using device: {device}")

if device.type == "cuda":
    # use bfloat16 for the entire notebook
    torch.autocast("cuda", dtype=torch.bfloat16).__enter__()
    # turn on tfloat32 for Ampere GPUs (https://pytorch.org/docs/stable/notes/cuda.html#tensorfloat-32-tf32-on-ampere-devices)
    if torch.cuda.get_device_properties(0).major >= 8:
        torch.backends.cuda.matmul.allow_tf32 = True
        torch.backends.cudnn.allow_tf32 = True
elif device.type == "mps":
    print(
        "\nSupport for MPS devices is preliminary. SAM 2 is trained with CUDA and might "
        "give numerically different outputs and sometimes degraded performance on MPS. "
        "See e.g. https://github.com/pytorch/pytorch/issues/84936 for a discussion."
    )

from sam2.build_sam import build_sam2, build_sam2_video_predictor
from sam2.sam2_image_predictor import SAM2ImagePredictor

scriptFolder = os.path.dirname(os.path.abspath(__file__))
sam2_checkpoint = modelFile
model_cfg = configFile

if preview_frame > -1:
    sam2_model = build_sam2(model_cfg, sam2_checkpoint, device=device)
    predictor = SAM2ImagePredictor(sam2_model)
else:
    predictor = build_sam2_video_predictor(model_cfg, sam2_checkpoint, device=device, vos_optimized=True)

def show_mask(mask, ax, obj_id=None, random_color=False):
    #if random_color:
    color = np.concatenate([np.random.random(3), np.array([0.6])], axis=0)
    #else:
    #    cmap = plt.get_cmap("tab10")
    #    cmap_idx = 0 if obj_id is None else obj_id
    #    color = np.array([*cmap(cmap_idx)[:3], 0.6])
    h, w = mask.shape[-2:]
    mask = mask.astype(np.uint8)
    mask_image = mask.reshape(h, w, 1) * color.reshape(1, 1, -1)
    if borders:
        import cv2
        contours, _ = cv2.findContours(mask,cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
        # Try to smooth contours
        contours = [cv2.approxPolyDP(contour, epsilon=0.01, closed=True) for contour in contours]
        mask_image = cv2.drawContours(mask_image, contours, -1, (1, 1, 1, 0.5), thickness=2)
    ax.imshow(mask_image)


def save_mask(mask, filename, obj_id=None):

    color = [255, 255, 255, 255]
    h, w = mask.shape[-2:]
    mask_image = mask.reshape(h, w, 1) * color
    if borders > 0:
        import cv2
        mask = mask.astype(np.uint8)
        contours, _ = cv2.findContours(mask,cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
        # Try to smooth contours
        contours = [cv2.approxPolyDP(contour, epsilon=0.01, closed=True) for contour in contours]
        mask_image = cv2.drawContours(mask_image, contours, -1, (200, 0, 0, 100), thickness=borders)

    pil_img = Image.fromarray(np.uint8(mask_image))
    pil_img.save(filename)

def show_points(coords, labels, ax, marker_size=200):
    pos_points = coords[labels==1]
    neg_points = coords[labels==0]
    ax.scatter(pos_points[:, 0], pos_points[:, 1], color='green', marker='*', s=marker_size, edgecolor='white', linewidth=1.25)
    ax.scatter(neg_points[:, 0], neg_points[:, 1], color='red', marker='*', s=marker_size, edgecolor='white', linewidth=1.25)


def show_box(box, ax):
    x0, y0 = box[0], box[1]
    w, h = box[2] - box[0], box[3] - box[1]
    #ax.add_patch(plt.Rectangle((x0, y0), w, h, edgecolor='green', facecolor=(0, 0, 0, 0), lw=2))

# scan all the JPEG frame names in this directory
frame_names = [
    p for p in os.listdir(inputFolder)
    if os.path.splitext(p)[-1] in [".jpg"]
]
frame_names.sort(key=lambda p: int(os.path.splitext(p)[0]))

# take a look the first video frame
#frame_idx = 0
#plt.figure(figsize=(9, 6))
#plt.title(f"frame {frame_idx}")
#plt.imshow(Image.open(os.path.join(video_dir, frame_names[frame_idx])))

ann_obj_id = 1  # give a unique id to each object we interact with (it can be any integers)

if preview_frame > -1:
    print(f"Init predictor on image: ", os.path.join(inputFolder, frame_names[preview_frame]))
    image = Image.open(os.path.join(inputFolder, frame_names[preview_frame]))
    image = np.array(image.convert("RGB"))
    predictor.set_image(image)
    #mask_input = logits[np.argmax(scores), :, :]
    masks, scores, logits = predictor.predict(
        point_coords=None if not points else points[preview_frame],
        point_labels=None if not labels else labels[preview_frame],
        box=None if not box else box[preview_frame],
        multimask_output=False)

    print(f"Saving preview image as: ", output_frame)
    save_mask((masks[0]), output_frame, ann_obj_id)
    sys.exit()
else:
    inference_state = predictor.init_state(video_path=inputFolder)


print(f"Adding points...")
# Let's add a positive click at (x, y) = (210, 350) to get started
#points = np.array([[423, 556], [250, 220]], dtype=np.float32)
# for labels, `1` means positive click and `0` means negative click
#labels = np.array([1, 1], np.int32)

first_list = list(points.keys())
in_first = set(first_list)
in_second = set(box.keys())
in_second_but_not_in_first = in_second - in_first
result = first_list + list(in_second_but_not_in_first)

for frame in result:
    _, _, out_mask_logits = predictor.add_new_points_or_box(
        inference_state=inference_state,
        frame_idx=frame,
        obj_id=ann_obj_id,
        box=None if not box else box[frame],
        points=None if not points else points[frame],
        labels=None if not labels else labels[frame]
    )

print(f"Adding points...DONE")

# show the results on the current (interacted) frame
# plt.figure(figsize=(9, 6))
# plt.title(f"frame {frame}")
# plt.imshow(Image.open(os.path.join(inputFolder, frame_names[frame])))
#show_points(points, labels, plt.gca())

# run propagation throughout the video and collect the results in a dict
video_segments = {}  # video_segments contains the per-frame segmentation results
for out_frame_idx, out_obj_ids, out_mask_logits in predictor.propagate_in_video(inference_state):
    video_segments[out_frame_idx] = {
        out_obj_id: (out_mask_logits[i] > 0.0).cpu().numpy()
        for i, out_obj_id in enumerate(out_obj_ids)
    }
print(f"Rendering all frames...")
# render the segmentation results every few frames
vis_frame_stride = 1
#plt.close("all")
for out_frame_idx in range(0, len(frame_names), vis_frame_stride):
    #plt.figure(figsize=(6, 4))
    #plt.title(f"frame {out_frame_idx}")
    #plt.imshow(Image.open(os.path.join(inputFolder, frame_names[out_frame_idx])))
    for out_obj_id, out_mask in video_segments[out_frame_idx].items():
        #show_mask(out_mask, plt.gca(), obj_id=out_obj_id)
        filename = output_frame + '/{:05d}'.format(out_frame_idx) + '.png'
        save_mask(out_mask, filename, obj_id=out_obj_id)
    #plt.show()

# Transform output png into video with alpha:
# ffmpeg -framerate 25 -pattern_type glob -i '*.png' -c:v ffv1 -pix_fmt yuva420p output.mkv

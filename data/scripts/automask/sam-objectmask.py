#!/usr/bin/env python3

import os
# if using Apple MPS, fall back to CPU for unsupported ops
# os.environ["PYTORCH_ENABLE_MPS_FALLBACK"] = "1"
import numpy as np
import torch
import matplotlib.pyplot as plt
import cv2
import sys
from PIL import Image

def process_csv(csv_string, resize):
    # Convert the CSV string back to a NumPy array
    np_array = np.fromstring(csv_string, dtype=int, sep=',')

    # Reshape the array if necessary (e.g., if it was a 2D array)
    if resize:
        cols = int((np.shape(np_array)[0])/2)
        print("Resize Array, COLS:")
        print(cols)
        np_array = np_array.reshape(cols, 2)

    print("NumPy Array:")
    print(np_array)
    return np_array

if __name__ == "__main__":
    if len(sys.argv) < 4:
        print("Usage: point_coordinates '200,250,300,255' labels '1,1' preview")
        sys.exit()
    else:
        csv_data = sys.argv[1]
        points = process_csv(csv_data, True)
        csv_data2 = sys.argv[2]
        labels = process_csv(csv_data2, False)
        preview = False
        if sys.argv[3] == 'preview':
            preview = True

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

from sam2.build_sam import build_sam2_video_predictor

sam2_checkpoint = "./sam2.1_hiera_tiny.pt"
model_cfg = "configs/sam2.1/sam2.1_hiera_t.yaml"

print(os.getcwd())

predictor = build_sam2_video_predictor(model_cfg, sam2_checkpoint, device=device)

def show_mask(mask, ax, obj_id=None, random_color=False):
    if random_color:
        color = np.concatenate([np.random.random(3), np.array([0.6])], axis=0)
    else:
        cmap = plt.get_cmap("tab10")
        cmap_idx = 0 if obj_id is None else obj_id
        color = np.array([*cmap(cmap_idx)[:3], 0.6])
    h, w = mask.shape[-2:]
    mask_image = mask.reshape(h, w, 1) * color.reshape(1, 1, -1)
    ax.imshow(mask_image)


def save_mask(mask, filename, obj_id=None):

    color = [255, 255, 255, 255]
    h, w = mask.shape[-2:]
    mask_image = mask.reshape(h, w, 1) * color
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
    ax.add_patch(plt.Rectangle((x0, y0), w, h, edgecolor='green', facecolor=(0, 0, 0, 0), lw=2))

# `video_dir` a directory of JPEG frames with filenames like `<frame_index>.jpg`
video_dir = "./videos/bedroom"

# scan all the JPEG frame names in this directory
frame_names = [
    p for p in os.listdir(video_dir)
    if os.path.splitext(p)[-1] in [".jpg", ".jpeg", ".JPG", ".JPEG"]
]
frame_names.sort(key=lambda p: int(os.path.splitext(p)[0]))

# take a look the first video frame
#frame_idx = 0
#plt.figure(figsize=(9, 6))
#plt.title(f"frame {frame_idx}")
#plt.imshow(Image.open(os.path.join(video_dir, frame_names[frame_idx])))


print(f"Init predictor...")
inference_state = predictor.init_state(video_path=video_dir)

ann_frame_idx = 0  # the frame index we interact with
ann_obj_id = 1  # give a unique id to each object we interact with (it can be any integers)

print(f"Adding points...")
# Let's add a positive click at (x, y) = (210, 350) to get started
#points = np.array([[423, 556], [250, 220]], dtype=np.float32)
points = np.array([[423, 556]], dtype=np.float32)
# for labels, `1` means positive click and `0` means negative click
#labels = np.array([1, 1], np.int32)
labels = np.array([1], np.int32)

_, out_obj_ids, out_mask_logits = predictor.add_new_points_or_box(
    inference_state=inference_state,
    frame_idx=ann_frame_idx,
    obj_id=ann_obj_id,
    points=points,
    labels=labels,
)

print(f"Adding points...DONE")

# show the results on the current (interacted) frame
# plt.figure(figsize=(9, 6))
# plt.title(f"frame {ann_frame_idx}")
# plt.imshow(Image.open(os.path.join(video_dir, frame_names[ann_frame_idx])))
#show_points(points, labels, plt.gca())
if preview:
    filename = '/tmp/preview.png'
    save_mask((out_mask_logits[0] > 0.0).cpu().numpy(), filename, obj_id=out_obj_ids[0])
    sys.exit()

# run propagation throughout the video and collect the results in a dict
video_segments = {}  # video_segments contains the per-frame segmentation results
for out_frame_idx, out_obj_ids, out_mask_logits in predictor.propagate_in_video(inference_state):
    video_segments[out_frame_idx] = {
        out_obj_id: (out_mask_logits[i] > 0.0).cpu().numpy()
        for i, out_obj_id in enumerate(out_obj_ids)
    }

# render the segmentation results every few frames
vis_frame_stride = 1
plt.close("all")
for out_frame_idx in range(0, len(frame_names), vis_frame_stride):
    #plt.figure(figsize=(6, 4))
    #plt.title(f"frame {out_frame_idx}")
    #plt.imshow(Image.open(os.path.join(video_dir, frame_names[out_frame_idx])))
    for out_obj_id, out_mask in video_segments[out_frame_idx].items():
        #show_mask(out_mask, plt.gca(), obj_id=out_obj_id)
        filename = 'color_img-' + '{:05d}'.format(out_frame_idx) + '.png'
        save_mask(out_mask, filename, obj_id=out_obj_id)
    #plt.show()

# Transform output png into video with alpha:
# ffmpeg -framerate 25 -pattern_type glob -i '*.png' -c:v ffv1 -pix_fmt yuva420p output.mkv

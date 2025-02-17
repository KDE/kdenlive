#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

import os
# if using Apple MPS, fall back to CPU for unsupported ops
# os.environ["PYTORCH_ENABLE_MPS_FALLBACK"] = "1"
import numpy as np
import torch
import sys
import argparse
from PIL import Image


def process_csv(array_data, csv_string, resize):
    # Convert the CSV string back to a NumPy array
    #resulting_array = {}
    vals_list = csv_string.split(';')
    for vals in vals_list:
        frame, csv_data = vals.split("=")
        np_array = np.fromstring(csv_data, dtype=int, sep=',')

        # Reshape the array if necessary (e.g., if it was a 2D array)
        if resize > 1:
            cols = int((np.shape(np_array)[0])/resize)
            np_array = np_array.reshape(cols, resize)
        array_data[int(frame)] = np_array

    #return array_data

if __name__ == "__main__":
    parser = argparse.ArgumentParser("SAM Object Mask Creator")
    parser.add_argument("-P", "--point_coordinates", help="Points coordinates with frame, like '0=200,250,300,255;100=10,50' for 2 points at frame 0 and one at frame 100")
    parser.add_argument("-F", "--preview_frame", help="The frame index for preview", default=-1)
    parser.add_argument("-L", "--labels", help="Points labels, 1 for include, 0 for exclude, like '0=1,0;100=1' for frame 0 and 100")
    parser.add_argument("-B", "--box_coordinates", help="Box coordinates with frame, like '0=10,20,150,255'")
    parser.add_argument("-I", "--inputFolder", help="folder where input jpg files are stored", default="/tmp/src-frames")
    parser.add_argument("-O", "--output", help="folder for rendered png image for preview of folder for rendering", default="/tmp/")
    parser.add_argument("-M", "--model", help="path for the model")
    parser.add_argument("-C", "--config", help="config for the model")
    parser.add_argument("-D", "--device", help="enforce a device: cuda, cpu")
    parser.add_argument('--offload', action='store_true')
    args = parser.parse_args()
    #if (args.point_coordinates is None or args.labels is None) and args.box_coordinates is None:
    #    config = vars(args)
    #    print(config)
    #    sys.exit()

    box = {}
    points = {}
    labels = {}
    if args.point_coordinates != None:
        process_csv(points, args.point_coordinates, 2)
        process_csv(labels, args.labels, 1)
    if args.box_coordinates != None:
        process_csv(box, args.box_coordinates, 4)
    preview_frame = int(args.preview_frame)
    if args.output != None:
        output_frame = args.output
    if args.inputFolder != None:
        inputFolder = args.inputFolder
    if args.model != None:
        modelFile = args.model
    if args.config != None:
        configFile = args.config
    if args.device != None:
        requestedDevice = args.device
    borders = 1

# select the device for computation
if requestedDevice != None:
    device = torch.device(requestedDevice)
    #if requestedDevice.startswith("cuda"):
        #print(f"Using CUDA version: {torch.version.cuda}")
elif torch.cuda.is_available():
    device = torch.device("cuda")
    #print(f"Using CUDA version: {torch.version.cuda}")
elif torch.backends.mps.is_available():
    device = torch.device("mps")
else:
    device = torch.device("cpu")

if device.type == "cuda":
    # Check available memory
    memInfo = torch.cuda.mem_get_info()
    print(f"GPU MEMINFO: {memInfo[0]} - {memInfo[1]}", file=sys.stdout, flush=True)
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

sam2_model = build_sam2(model_cfg, sam2_checkpoint, device=device)
predictor = SAM2ImagePredictor(sam2_model)

def save_mask(mask, filename, obj_id=None):
    color = [255, 100, 100, 180]
    h, w = mask.shape[-2:]
    mask = mask.astype(np.uint8)
    mask_image = mask.reshape(h, w, 1) * color
    #print(f"Saving mask: {filename}")
    if borders > 0:
        import cv2
        #mask = mask.astype(np.uint8)
        try:
            contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
            # Try to smooth contours
            contours = [cv2.approxPolyDP(contour, epsilon=0.01, closed=True) for contour in contours]
            color2 = [255, 0, 0, 255]
            #color2[3] = 255
            mask_image = cv2.drawContours(mask_image, contours, -1, color2, borders)
        except:
            print("skipping contour", file=sys.stdout, flush=True)

    pil_img = Image.fromarray(np.uint8(mask_image))
    pil_img.save(filename)


def show_points(coords, labels, ax, marker_size=200):
    pos_points = coords[labels == 1]
    neg_points = coords[labels == 0]
    ax.scatter(pos_points[:, 0], pos_points[:, 1], color='green', marker='*', s=marker_size, edgecolor='white', linewidth=1.25)
    ax.scatter(neg_points[:, 0], neg_points[:, 1], color='red', marker='*', s=marker_size, edgecolor='white', linewidth=1.25)

# scan all the JPEG frame names in this directory
frame_names = [
    p for p in os.listdir(inputFolder)
    if os.path.splitext(p)[-1] in [".jpg"]
]
frame_names.sort(key=lambda p: int(os.path.splitext(p)[0]))

def generate_preview(predictor):
    if predictor == None:
        predictor = SAM2ImagePredictor(sam2_model)

    image = Image.open(os.path.join(inputFolder, frame_names[preview_frame]))
    image = np.array(image.convert("RGB"))
    predictor.set_image(image)
    #mask_input = logits[np.argmax(scores), :, :]
    masks, scores, logits = predictor.predict(
        point_coords=None if not points else points[preview_frame],
        point_labels=None if not labels else labels[preview_frame],
        box=None if not box else box[preview_frame],
        multimask_output=False)
    filename = output_frame + '/preview-{:05d}'.format(preview_frame) + '.png'
    save_mask((masks[0]), filename, ann_obj_id)
    print(f"preview ok {preview_frame}", file=sys.stdout, flush=True)

def render_video():
    # run propagation throughout the video and collect the results in a dict
    video_segments = {}  # video_segments contains the per-frame segmentation results
    print("INFO:Propagating in video\n", file=sys.stdout, flush=True)
    for out_frame_idx, out_obj_ids, out_mask_logits in videoPredictor.propagate_in_video(inference_state):
        video_segments[out_frame_idx] = {
            out_obj_id: (out_mask_logits[i] > 0.0).cpu().numpy()
            for i, out_obj_id in enumerate(out_obj_ids)
        }
    # render the segmentation results every few frames
    vis_frame_stride = 1
    print("INFO:Exporting frames\n", file=sys.stdout, flush=True)
    framesCount = len(frame_names)
    for out_frame_idx in range(0, framesCount, vis_frame_stride):
        #plt.figure(figsize=(6, 4))
        #plt.title(f"frame {out_frame_idx}")
        #plt.imshow(Image.open(os.path.join(inputFolder, frame_names[out_frame_idx])))
        for out_obj_id, out_mask in video_segments[out_frame_idx].items():
            filename = output_frame + '/{:05d}'.format(out_frame_idx) + '.png'
            save_mask(out_mask, filename, obj_id=out_obj_id)
        if framesCount > 100:
            percent = int(100 * out_frame_idx / framesCount)
            print(f"Export {percent}%|\n", file=sys.stderr, flush=True)

# take a look the first video frame
#frame_idx = 0
#plt.figure(figsize=(9, 6))
#plt.title(f"frame {frame_idx}")
#plt.imshow(Image.open(os.path.join(video_dir, frame_names[frame_idx])))
videoPredictor_initialized = False
ann_obj_id = 1  # give a unique id to each object we interact with (it can be any integers)

if device.type == "cuda" and torch.cuda.get_device_properties(0).major >= 8:
    videoPredictor = build_sam2_video_predictor(model_cfg, sam2_checkpoint, device=device)  #, vos_optimized=True)
else:
    videoPredictor = build_sam2_video_predictor(model_cfg, sam2_checkpoint, device=device)

while 1:
    line = sys.stdin.readline().rstrip()

    if line.startswith("preview="):
        # Generate image preview
        inArgs = parser.parse_args(line[8:].split())
        if inArgs.point_coordinates != None:
            process_csv(points, inArgs.point_coordinates, 2)
            process_csv(labels, inArgs.labels, 1)
        if inArgs.box_coordinates != None:
            process_csv(box, inArgs.box_coordinates, 4)
        preview_frame = int(inArgs.preview_frame)
        generate_preview(predictor)

        # get ready for rendering
        if videoPredictor_initialized == False:
            if args.offload:
                print("Offloading video to CPU\n", file=sys.stdout, flush=True)
            inference_state = videoPredictor.init_state(video_path=inputFolder, async_loading_frames=True, offload_video_to_cpu=args.offload)
            videoPredictor_initialized = True

    if line.startswith("render="):
        if videoPredictor_initialized == False:
            print("STILL LOADING FRAMES...\n", file=sys.stdout, flush=True)
            continue
        # Destroy image predictor
        del predictor
        predictor = None
        # Generate output frames
        output_frame = line[7:].rstrip()
        first_list = list(points.keys())
        in_first = set(first_list)
        in_second = set(box.keys())
        in_second_but_not_in_first = in_second - in_first
        result = first_list + list(in_second_but_not_in_first)

        for frame in result:
            _, _, out_mask_logits = videoPredictor.add_new_points_or_box(
                inference_state=inference_state,
                frame_idx=frame,
                obj_id=ann_obj_id,
                box=None if not box else box[frame],
                points=None if not points else points[frame],
                labels=None if not labels else labels[frame]
            )
        render_video()
        print("mask ok", file=sys.stdout, flush=True)
        del videoPredictor
        videoPredictor_initialized = False
        sys.exit()

    if line == "q":
        print("CLOSING...\n", file=sys.stdout, flush=True)
        sys.exit()


#with torch.inference_mode(), torch.autocast("cuda", dtype=torch.bfloat16):


# Let's add a positive click at (x, y) = (210, 350) to get started
#points = np.array([[423, 556], [250, 220]], dtype=np.float32)
# for labels, `1` means positive click and `0` means negative click
#labels = np.array([1, 1], np.int32)



# show the results on the current (interacted) frame
# plt.figure(figsize=(9, 6))
# plt.title(f"frame {frame}")
# plt.imshow(Image.open(os.path.join(inputFolder, frame_names[frame])))
#show_points(points, labels, plt.gca())

    #plt.show()

# Transform output png into video with alpha:
# ffmpeg -framerate 25 -pattern_type glob -i '*.png' -c:v ffv1 -pix_fmt yuva420p output.mkv

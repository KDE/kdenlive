# SPDX-FileCopyrightText: Meta Platforms, Inc. and affiliates.
# SPDX-FileCopyrightText: Kdenlive contributors
# SPDX-License-Identifier: Apache-2.0 OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

from collections import OrderedDict
import os
import torch

from sam2.sam2_video_predictor import SAM2VideoPredictor as SAM2VideoPredictorOfficial
from sam2.utils.misc import  _load_img_as_tensor


def load_video_frames_from_jpg_images(
    jpg_folder: str,
    image_size,
    offload_video_to_cpu,
    img_mean=(0.485, 0.456, 0.406),
    img_std=(0.229, 0.224, 0.225),
    compute_device=torch.device("cuda"),
):
    """
    Load the video frames from a directory of JPEG files ("<frame_index>.jpg" format).

    The frames are resized to image_size x image_size and are loaded to GPU if
    `offload_video_to_cpu` is `False` and to CPU if `offload_video_to_cpu` is `True`.

    You can load a frame asynchronously by setting `async_loading_frames` to `True`.
    """
    frame_names = [
        p
        for p in os.listdir(jpg_folder)
        if os.path.splitext(p)[-1] in [".jpg", ".jpeg", ".JPG", ".JPEG"]
    ]
    frame_names.sort(key=lambda p: int(os.path.splitext(p)[0]))
    num_frames = len(frame_names)
    if num_frames == 0:
        raise RuntimeError(f"no images found in {jpg_folder}")
    img_paths = [os.path.join(jpg_folder, frame_name) for frame_name in frame_names]
    img_mean = torch.tensor(img_mean, dtype=torch.float32)[:, None, None]
    img_std = torch.tensor(img_std, dtype=torch.float32)[:, None, None]

    lazy_images = AsyncVideoFrameLoader(
        img_paths,
        image_size,
        offload_video_to_cpu,
        img_mean,
        img_std,
        compute_device,
    )
    return lazy_images, lazy_images.video_height, lazy_images.video_width

class AsyncVideoFrameLoader:
    """
    A list of video frames to be load asynchronously without blocking session start.
    """

    def __init__(
        self,
        img_paths,
        image_size,
        offload_video_to_cpu,
        img_mean,
        img_std,
        compute_device,
    ):
        self.img_paths = img_paths
        self.image_size = image_size
        self.offload_video_to_cpu = offload_video_to_cpu
        self.img_mean = img_mean
        self.img_std = img_std
        # items in `self.images` will be loaded asynchronously
        self.images = [None] * len(img_paths)
        # catch and raise any exceptions in the async loading thread
        self.exception = None
        # video_height and video_width be filled when loading the first image
        self.video_height = None
        self.video_width = None
        self.compute_device = compute_device

        # load the first frame to fill video_height and video_width and also
        # to cache it (since it's most likely where the user will click)
        self.__getitem__(0)

    def __getitem__(self, index):
        if self.exception is not None:
            raise RuntimeError("Failure in frame loading thread") from self.exception

        img = self.images[index]
        if img is not None:
            return img

        img, video_height, video_width = _load_img_as_tensor(
            self.img_paths[index], self.image_size
        )
        self.video_height = video_height
        self.video_width = video_width
        # normalize by mean and std
        img -= self.img_mean
        img /= self.img_std
        if not self.offload_video_to_cpu:
            img = img.to(self.compute_device, non_blocking=True)
        if index == 0:
            # TODO (SAM): official loader from SAM2 caches all frames in memory which prevents it from being used on longer videos (more than a few seconds)
            #  Now we only cache the first frame, but we should experiment with caching more frames depending on length of the video and memory availability
            self.images[index] = img
        return img

    def __len__(self):
        return len(self.images)

class SAM2VideoPredictor(SAM2VideoPredictorOfficial):
    """The predictor class to handle user interactions and manage inference states."""

    def __init__(
        self,
        **kwargs,
    ):
        super().__init__(**kwargs)

    @torch.inference_mode()
    def init_state(
        self,
        video_path,
        offload_video_to_cpu=False,
        offload_state_to_cpu=False,
        async_loading_frames=True, # ignored, we'll always load frames asynchronously
    ):
        """Initialize an inference state."""
        assert os.path.isdir(video_path), "Video path must be a directory"

        compute_device = self.device  # device of the model
        images, video_height, video_width = load_video_frames_from_jpg_images(
            jpg_folder=video_path,
            image_size=self.image_size,
            offload_video_to_cpu=offload_video_to_cpu,
            compute_device=compute_device,
        )
        inference_state = {}
        inference_state["images"] = images
        inference_state["num_frames"] = len(images)
        # whether to offload the video frames to CPU memory
        # turning on this option saves the GPU memory with only a very small overhead
        inference_state["offload_video_to_cpu"] = offload_video_to_cpu
        # whether to offload the inference state to CPU memory
        # turning on this option saves the GPU memory at the cost of a lower tracking fps
        # (e.g. in a test case of 768x768 model, fps dropped from 27 to 24 when tracking one object
        # and from 24 to 21 when tracking two objects)
        inference_state["offload_state_to_cpu"] = offload_state_to_cpu
        # the original video height and width, used for resizing final output scores
        inference_state["video_height"] = video_height
        inference_state["video_width"] = video_width
        inference_state["device"] = compute_device
        if offload_state_to_cpu:
            inference_state["storage_device"] = torch.device("cpu")
        else:
            inference_state["storage_device"] = compute_device
        # inputs on each frame
        inference_state["point_inputs_per_obj"] = {}
        inference_state["mask_inputs_per_obj"] = {}
        # visual features on a small number of recently visited frames for quick interactions
        inference_state["cached_features"] = {}
        # values that don't change across frames (so we only need to hold one copy of them)
        inference_state["constants"] = {}
        # mapping between client-side object id and model-side object index
        inference_state["obj_id_to_idx"] = OrderedDict()
        inference_state["obj_idx_to_id"] = OrderedDict()
        inference_state["obj_ids"] = []
        # Slice (view) of each object tracking results, sharing the same memory with "output_dict"
        inference_state["output_dict_per_obj"] = {}
        # A temporary storage to hold new outputs when user interact with a frame
        # to add clicks or mask (it's merged into "output_dict" before propagation starts)
        inference_state["temp_output_dict_per_obj"] = {}
        # Frames that already holds consolidated outputs from click or mask inputs
        # (we directly use their consolidated outputs during tracking)
        # metadata for each tracking frame (e.g. which direction it's tracked)
        inference_state["frames_tracked_per_obj"] = {}
        # Warm up the visual backbone and cache the image feature on frame 0
        self._get_image_feature(inference_state, frame_idx=0, batch_size=1)
        return inference_state

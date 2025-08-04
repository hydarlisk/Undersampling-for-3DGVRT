import os
import torch
from torchvision.io import read_image
from torchvision.transforms.functional import convert_image_dtype, resize
from torchmetrics import PeakSignalNoiseRatio
from torchmetrics.image import StructuralSimilarityIndexMeasure
from torchmetrics.image.lpip import LearnedPerceptualImagePatchSimilarity

from tqdm import tqdm

# Metric 초기화
psnr = PeakSignalNoiseRatio(data_range=1.0)
ssim = StructuralSimilarityIndexMeasure(data_range=1.0)
lpips = LearnedPerceptualImagePatchSimilarity(net_type='vgg')

# 비교 대상 폴더
ref_dir = 'GroundTruth/hotdog'
cmp_dir = 'hotdog/0804/'
cmp_dirs = ['hotdog/3dgrt', cmp_dir+'FRT', cmp_dir+'us_150', cmp_dir+'us_298']

# 이미지 개수
num_images = 200  # ← 필요한 만큼 수정하세요

# 결과 저장용
results = {cmp: {'psnr': [], 'ssim': [], 'lpips': []} for cmp in cmp_dirs}

# load ref imgs
for i in tqdm(range(num_images)):
    file_name = f"r_{i}.png"
    ref_path = os.path.join(ref_dir, file_name)
    if not os.path.exists(ref_path):
        print(f"⚠️ {ref_path} 없음, 건너뜀")
        continue

    ref_img = read_image(ref_path).float() / 255.0  # [0,1] 정규화
    ref_img = ref_img.unsqueeze(0)  # [1, C, H, W]
    if ref_img.shape[1] == 4:
        ref_img = ref_img[:, :3]

    # load cmp imgs and compare with ref imgs
    for cmp in cmp_dirs:
        cmp_path = os.path.join(cmp, file_name)
        if not os.path.exists(cmp_path):
            print(f"⚠️ {cmp_path} 없음, 건너뜀")
            continue

        cmp_img = read_image(cmp_path).float() / 255.0
        cmp_img = cmp_img.unsqueeze(0)
        if cmp_img.shape[1] == 4:
            cmp_img = cmp_img[:, :3]

        if ref_img.shape != cmp_img.shape:
            cmp_img = resize(cmp_img, ref_img.shape[-2:])

        results[cmp]['psnr'].append(psnr(ref_img, cmp_img).item())
        results[cmp]['ssim'].append(ssim(ref_img, cmp_img).item())
        # results[cmp]['lpips'].append(lpips(ref_img, cmp_img).item())

# 평균 결과 출력
print("\n📊 평균 결과:")
for cmp in cmp_dirs:
    psnr_avg = sum(results[cmp]['psnr']) / len(results[cmp]['psnr']) if results[cmp]['psnr'] else 0
    ssim_avg = sum(results[cmp]['ssim']) / len(results[cmp]['ssim']) if results[cmp]['ssim'] else 0
    lpips_avg = sum(results[cmp]['lpips']) / len(results[cmp]['lpips']) if results[cmp]['lpips'] else 0
    print(f"\n▶ {cmp} vs a")
    print(f"PSNR : {psnr_avg:.2f}")
    print(f"SSIM : {ssim_avg:.4f}")
    print(f"LPIPS: {lpips_avg:.4f}")

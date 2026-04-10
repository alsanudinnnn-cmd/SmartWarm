const smartWarmCloudinaryConfig = {
  cloudName: "drgxibq5q",
  uploadPreset: "smartwarmdb"
};

window.smartWarmCloudinary = {
  async uploadPlayerPhoto(file) {
    const formData = new FormData();
    formData.append("file", file);
    formData.append("upload_preset", smartWarmCloudinaryConfig.uploadPreset);

    const response = await fetch(
      `https://api.cloudinary.com/v1_1/${smartWarmCloudinaryConfig.cloudName}/image/upload`,
      {
        method: "POST",
        body: formData
      }
    );

    const data = await response.json();

    if (!response.ok || !data.secure_url) {
      throw new Error(data.error?.message || "Cloudinary upload failed");
    }

    return data;
  }
};

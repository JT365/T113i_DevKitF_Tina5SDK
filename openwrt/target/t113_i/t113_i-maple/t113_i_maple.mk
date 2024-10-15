$(call inherit-product-if-exists, target/allwinner/t113_i-common/t113_i-common.mk)

PRODUCT_PACKAGES +=

PRODUCT_COPY_FILES +=

PRODUCT_AAPT_CONFIG := large xlarge hdpi xhdpi
PRODUCT_AAPT_PERF_CONFIG := xhdpi
PRODUCT_CHARACTERISTICS := musicbox

PRODUCT_BRAND := allwinner
PRODUCT_NAME := t113_i-maple
PRODUCT_DEVICE := t113_i-maple
PRODUCT_MODEL := Allwinner t113 maple board

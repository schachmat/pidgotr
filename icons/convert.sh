od -v -t x1 "${1:--}" | head -n -1 | sed -e 's|^[0-9]*|"|' -e 's| |\\x|g' -e 's|$|"|' -e '$s|$|;|'

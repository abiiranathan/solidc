# Clone the BlocksRuntime repository
git clone --depth 1 https://github.com/mackyle/blocksruntime.git
cd blocksruntime

# Build using filcc via the repository's build script
CC=/opt/fil/bin/filcc ./buildlib

# Install the Fil-C built library and headers to /opt/fil
sudo prefix=/opt/fil ./installlib
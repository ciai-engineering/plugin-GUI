#!/bin/bash
# Redis DataThread Plugin Installation Script

set -e

echo "🔧 Redis DataThread Plugin Installation"
echo "======================================="

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ] || [ ! -d "Source" ]; then
    echo "❌ Error: This script must be run from the OpenEphys GUI root directory"
    echo "   Expected structure: CMakeLists.txt, Source/, Plugins/"
    exit 1
fi

# Check if plugin directory exists
if [ ! -d "Plugins/RedisDataThread" ]; then
    echo "❌ Error: RedisDataThread plugin directory not found"
    echo "   Expected: Plugins/RedisDataThread/"
    exit 1
fi

echo "✓ Found OpenEphys GUI directory structure"

# Check for hiredis library
echo ""
echo "📦 Checking dependencies..."

HIREDIS_FOUND=false

# Check with pkg-config
if pkg-config --exists hiredis 2>/dev/null; then
    echo "✓ Found hiredis via pkg-config"
    HIREDIS_FOUND=true
# Check for header files
elif [ -f "/usr/include/hiredis/hiredis.h" ] || [ -f "/usr/local/include/hiredis/hiredis.h" ]; then
    echo "✓ Found hiredis header files"
    HIREDIS_FOUND=true
else
    echo "⚠️  hiredis library not found"
    echo "   The plugin will compile with stub implementation"
    echo ""
    echo "   To install hiredis:"
    echo "   Ubuntu/Debian: sudo apt-get install libhiredis-dev"
    echo "   macOS: brew install hiredis"
    echo "   CentOS/RHEL: sudo yum install hiredis-devel"
fi

# Create build directory if it doesn't exist
if [ ! -d "Build" ]; then
    echo ""
    echo "📁 Creating build directory..."
    mkdir Build
fi

cd Build

# Configure with CMake
echo ""
echo "⚙️  Configuring build..."
cmake -DCMAKE_BUILD_TYPE=Release .. || {
    echo "❌ CMake configuration failed"
    exit 1
}

# Build the plugin
echo ""
echo "🔨 Building RedisDataThread plugin..."
make RedisDataThread -j$(nproc 2>/dev/null || echo 4) || {
    echo "❌ Build failed"
    exit 1
}

# Check if plugin was built successfully
PLUGIN_PATH="Release/plugins/RedisDataThread.so"
if [ -f "$PLUGIN_PATH" ]; then
    PLUGIN_SIZE=$(stat -c%s "$PLUGIN_PATH" 2>/dev/null || stat -f%z "$PLUGIN_PATH" 2>/dev/null || echo "unknown")
    echo "✓ Plugin built successfully: $PLUGIN_PATH ($PLUGIN_SIZE bytes)"
else
    echo "❌ Plugin file not found: $PLUGIN_PATH"
    exit 1
fi

# Check plugin symbols
echo ""
echo "🔍 Verifying plugin symbols..."
if command -v nm >/dev/null 2>&1; then
    if nm -D "$PLUGIN_PATH" 2>/dev/null | grep -q "getLibInfo\|getPluginInfo"; then
        echo "✓ Plugin exports correct symbols"
    else
        echo "⚠️  Warning: Plugin symbols not found (this might be normal)"
    fi
else
    echo "⚠️  nm command not available, skipping symbol check"
fi

# Installation summary
echo ""
echo "🎉 Installation completed successfully!"
echo ""
echo "📋 Next steps:"
echo "   1. Start the OpenEphys GUI: ./Release/open-ephys"
echo "   2. Add 'Redis Source' to your signal chain"
echo "   3. Configure Redis connection settings"

if [ "$HIREDIS_FOUND" = true ]; then
    echo "   4. Install and start Redis server:"
    echo "      sudo apt-get install redis-server  # Ubuntu/Debian"
    echo "      brew install redis                 # macOS"
    echo "   5. Test with example script:"
    echo "      python3 ../Plugins/RedisDataThread/examples/redis_data_sender.py"
else
    echo "   4. Install hiredis library for full functionality"
    echo "   5. Rebuild plugin after installing hiredis"
fi

echo ""
echo "📚 Documentation: Plugins/RedisDataThread/README.md"
echo "🧪 Test script: python3 ../test_redis_plugin.py"

cd ..
echo ""
echo "✅ Installation complete!"

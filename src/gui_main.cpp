// Slider window for interactive resizing (assignment bonus item 7b):
//   seamgui <input> <output>
//
// Built only with -DSEAM_BUILD_GUI=ON (uses Dear ImGui + GLFW + OpenGL).
//
// How it stays instant: when the image is opened, the order in which carve() removes vertical
// seams is computed ONCE in a background thread (computeVerticalSeamOrder). After that, any
// width is just "keep the pixels removed later than step W - w" (applyVerticalOrder), O(W*H).
// When the width slider is released, the horizontal seam order for that width is computed the
// same way, and then the height slider is instant too. Every result equals carve() exactly
// (see the "order matches carve" tests).

#include "seam/carver.hpp"
#include "seam/image.hpp"
#include "seam/interactive.hpp" // outputNameForSize
#include "seam/seam_order.hpp"

#include <GLFW/glfw3.h> // also includes the OpenGL header
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl2.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <exception>
#include <future>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

// Seam-finding methods the user can pick in the window.
struct MethodChoice {
    const char* label;
    seam::Method method;
};
const MethodChoice kMethods[] = {
    {"Dynamic programming", seam::Method::DynamicProgramming},
    {"Greedy", seam::Method::Greedy},
    {"Graph cut (slow: small images only)", seam::Method::GraphCut},
};

// Thrown from a progress callback to stop a background computation early.
struct Cancelled : std::exception {};

// One seam-order computation running in a background thread, with its progress.
struct BackgroundOrder {
    std::future<seam::SeamOrder> result;
    std::shared_ptr<std::atomic<int>> done = std::make_shared<std::atomic<int>>(0);
    std::shared_ptr<std::atomic<int>> total = std::make_shared<std::atomic<int>>(1);
    std::shared_ptr<std::atomic<bool>> cancel = std::make_shared<std::atomic<bool>>(false);
    int forWidth = 0; // horizontal orders: the width this order belongs to

    bool running() const { return result.valid(); }
    bool finished() const {
        return result.valid() && result.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }
    float fraction() const { return static_cast<float>(*done) / static_cast<float>(std::max(1, total->load())); }
};

// Starts computing a vertical (or horizontal) seam order of `img` down to 1 pixel, in a thread.
BackgroundOrder startOrder(const seam::Image& img, seam::Method method, bool horizontal) {
    BackgroundOrder task;
    // The thread gets its own copies of the progress counters (shared_ptr), so they stay
    // valid even if `task` is replaced while the thread is still running.
    auto done = task.done;
    auto total = task.total;
    auto cancel = task.cancel;
    task.result = std::async(std::launch::async, [img, method, horizontal, done, total, cancel]() {
        const seam::ProgressCallback progress = [done, total, cancel](int d, int t) {
            *done = d;
            *total = t;
            if (*cancel) {
                throw Cancelled(); // unwinds carve() and ends the thread early
            }
        };
        return horizontal ? seam::computeHorizontalSeamOrder(img, 1, method, 1, progress)
                          : seam::computeVerticalSeamOrder(img, 1, method, 1, progress);
    });
    return task;
}

// Stops a task (if running) and waits for its thread to end.
void stopTask(BackgroundOrder& task) {
    if (task.running()) {
        *task.cancel = true;
        try {
            task.result.get();
        } catch (...) {
            // Cancelled (or failed): the result is not needed any more.
        }
    }
}

// Everything that decides what picture is on screen. When it changes, the picture is rebuilt.
struct View {
    int width = -1;
    int height = -1;
    int method = -1;
    bool showSeams = false;
    bool verticalReady = false;
    bool heightReady = false;

    bool operator!=(const View& o) const {
        return width != o.width || height != o.height || method != o.method || showSeams != o.showSeams ||
               verticalReady != o.verticalReady || heightReady != o.heightReady;
    }
};

// Copies an image into an OpenGL texture so ImGui can draw it.
void uploadTexture(GLuint texture, const seam::Image& img) {
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // rows are 3 * width bytes, not padded to 4
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img.width, img.height, 0, GL_RGB, GL_UNSIGNED_BYTE, img.data.data());
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: seamgui <input> <output>\n"
                     "  Opens a window with width/height sliders. 'Save' writes <output>_WxH.\n";
        return 1;
    }
    const std::string outputPath = argv[2];
    seam::Image original;
    try {
        original = seam::loadImage(argv[1]);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    // ---- window + ImGui setup (standard Dear ImGui GLFW/OpenGL2 boilerplate)
    if (!glfwInit()) {
        std::cerr << "Error: could not start GLFW\n";
        return 1;
    }
    GLFWwindow* window = glfwCreateWindow(1280, 800, "Seam carving", nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "Error: could not open a window\n";
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync: redraw at the screen's refresh rate, not as fast as possible
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr; // don't write imgui.ini into the working folder
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL2_Init();

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); // sharp pixels when zoomed in

    // ---- state
    int methodIndex = 0;
    BackgroundOrder verticalTask = startOrder(original, kMethods[methodIndex].method, false);
    seam::SeamOrder vertical;     // ready when verticalReady
    bool verticalReady = false;
    BackgroundOrder horizontalTask;
    seam::SeamOrder horizontal;   // valid for width horizontalWidth
    int horizontalWidth = -1;
    std::string error;            // e.g. "Greedy seam finder is not implemented yet"

    int width = original.width;   // slider values
    int height = original.height;
    bool showSeams = false;
    seam::Image narrow = original; // the image at the current width (full height)
    int narrowWidth = original.width;
    seam::Image result = original; // the image at width x height
    View shownView;                // what the texture currently shows (starts invalid)
    std::string saveMessage;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // ---- collect finished background work
        if (verticalTask.finished()) {
            try {
                vertical = verticalTask.result.get();
                verticalReady = true;
            } catch (const std::exception& e) {
                error = e.what();
            }
        }
        if (horizontalTask.finished()) {
            try {
                horizontal = horizontalTask.result.get();
                horizontalWidth = horizontalTask.forWidth;
            } catch (const std::exception& e) {
                error = e.what();
            }
        }

        // ---- UI
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("Seam carving", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

        ImGui::BeginChild("controls", ImVec2(330, 0));
        ImGui::Text("Original: %d x %d", original.width, original.height);
        ImGui::Text("Result:   %d x %d", result.width, result.height);
        ImGui::Separator();

        // Method: changing it throws away both orders and starts again.
        if (ImGui::Combo("Method", &methodIndex, [](void*, int i) { return kMethods[i].label; }, nullptr,
                         static_cast<int>(std::size(kMethods)))) {
            stopTask(verticalTask);
            stopTask(horizontalTask);
            verticalTask = startOrder(original, kMethods[methodIndex].method, false);
            verticalReady = false;
            horizontalWidth = -1;
            error.clear();
            width = original.width; // back to full size until the new order is ready
            height = original.height;
            narrow = original;
            narrowWidth = original.width;
        }

        // Width slider: usable once the vertical order is ready.
        if (!verticalReady && error.empty()) {
            ImGui::ProgressBar(verticalTask.fraction(), ImVec2(-1, 0), "Preparing width slider...");
        }
        ImGui::BeginDisabled(!verticalReady);
        ImGui::SliderInt("Width", &width, 1, original.width);
        const bool draggingWidth = ImGui::IsItemActive();
        ImGui::EndDisabled();

        // Height slider: usable once the horizontal order for THIS width is ready.
        const bool heightReady = verticalReady && horizontalWidth == width;
        if (verticalReady && !heightReady && error.empty()) {
            ImGui::ProgressBar(horizontalTask.running() ? horizontalTask.fraction() : 0.0f, ImVec2(-1, 0),
                               "Preparing height slider...");
        }
        if (!heightReady) {
            height = original.height; // full height until the order for this width exists
        }
        ImGui::BeginDisabled(!heightReady);
        ImGui::SliderInt("Height", &height, 1, original.height);
        ImGui::EndDisabled();

        ImGui::Checkbox("Show removed seams on original", &showSeams);
        ImGui::TextDisabled("red = vertical, blue = horizontal");
        if (ImGui::Button("Reset")) {
            width = original.width;
            height = original.height;
        }
        ImGui::SameLine();
        if (ImGui::Button("Save")) {
            try {
                const std::string path = seam::outputNameForSize(outputPath, result.width, result.height);
                seam::saveImage(result, path);
                saveMessage = "Saved " + path;
            } catch (const std::exception& e) {
                saveMessage = std::string("Error: ") + e.what();
            }
        }
        if (!saveMessage.empty()) {
            ImGui::TextWrapped("%s", saveMessage.c_str());
        }
        if (!error.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.4f, 0.4f, 1)); // red
            ImGui::TextWrapped("Error: %s", error.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::Separator();
        ImGui::TextWrapped("Seams are found once per method (vertical) and once per width (horizontal), "
                           "so moving a slider only picks which pixels to keep. Each result is exactly "
                           "what carve() gives for that size.");
        ImGui::EndChild();

        // ---- rebuild the picture if something changed
        if (verticalReady && narrowWidth != width) {
            narrow = seam::applyVerticalOrder(original, vertical, width);
            narrowWidth = width;
        }
        // Start the horizontal order for this width once the user lets go of the width slider.
        // (Only one runs at a time; a stale one is cancelled first.)
        if (verticalReady && !draggingWidth && horizontalWidth != width && error.empty()) {
            if (horizontalTask.running() && horizontalTask.forWidth != width) {
                stopTask(horizontalTask);
            }
            if (!horizontalTask.running()) {
                horizontalTask = startOrder(narrow, kMethods[methodIndex].method, true);
                horizontalTask.forWidth = width;
            }
        }
        View view;
        view.width = width;
        view.height = height;
        view.method = methodIndex;
        view.showSeams = showSeams;
        view.verticalReady = verticalReady;
        view.heightReady = heightReady;
        if (view != shownView) {
            result = heightReady && height != original.height ? seam::applyHorizontalOrder(narrow, horizontal, height)
                                                              : narrow;
            if (showSeams && verticalReady) {
                // While the height slider is not ready, no horizontal seams are shown.
                seam::SeamOrder noHorizontal;
                noHorizontal.width = width;
                noHorizontal.height = original.height;
                noHorizontal.step.assign(static_cast<std::size_t>(width) * original.height, 0);
                uploadTexture(texture, seam::paintRemovedPixels(original, vertical, width,
                                                                heightReady ? horizontal : noHorizontal,
                                                                heightReady ? height : original.height));
            } else {
                uploadTexture(texture, result);
            }
            shownView = view;
        }

        // ---- draw the picture at a fixed scale, so it visibly shrinks as the sliders move
        ImGui::SameLine();
        ImGui::BeginChild("image");
        const ImVec2 space = ImGui::GetContentRegionAvail();
        const float scale = std::min({space.x / static_cast<float>(original.width),
                                      space.y / static_cast<float>(original.height), 3.0f});
        const seam::Image& shown = showSeams ? original : result;
        ImGui::Image(static_cast<ImTextureID>(texture),
                     ImVec2(static_cast<float>(shown.width) * scale, static_cast<float>(shown.height) * scale));
        ImGui::EndChild();
        ImGui::End();

        ImGui::Render();
        int fbWidth = 0;
        int fbHeight = 0;
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
        glViewport(0, 0, fbWidth, fbHeight);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // ---- clean up: stop background threads before the data they use goes away
    stopTask(verticalTask);
    stopTask(horizontalTask);
    glDeleteTextures(1, &texture);
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

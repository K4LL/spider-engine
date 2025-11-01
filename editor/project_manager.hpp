#pragma once
#include <string>
#include <fstream>

#include "definitions.hpp"

#include "flat_hash_map.hpp"

namespace spider_engine::hub {
    class ProjectManager {
    private:
        using ProjectName = std::string;
        using ProjectPath = std::string;

        ska::flat_hash_map<ProjectName, ProjectPath> projects_;

        std::filesystem::path currentDirectory;

        const char* filename = "projects.sm";

        void saveProject(const std::string& name, const std::string& path)
        {
            std::ofstream file(filename, std::ios::app);
            if (!file) return;

            file << name << "\n";
            file << path << "\n";
        }

        void loadProjects()
        {
            std::ifstream file(filename);
            if (!file) return;

            std::string name, path;
            while (true) {
                if (!std::getline(file, name)) break;
                if (!std::getline(file, path)) break;
                projects_.emplace(name, path);
            }
        }

    public:
        ProjectManager() {
            currentDirectory = std::filesystem::current_path();
            loadProjects();
        }

        void clearSavedProjects() {
            std::ofstream file(filename, std::ios::trunc);
        }

        void addProject(const ProjectName& name,
                        const ProjectPath& path)
        {
            projects_.emplace(name, path);
            saveProject(name, path);
        }
        void removeProject(const std::string& name) {
            auto it = projects_.erase(name);

            std::ofstream file(filename, std::ios::trunc);
            for (auto& [n, p] : projects_) {
                file << n << "\n" << p << "\n";
            }
        }

        const auto& getProjects() const { return projects_; }
    };

}
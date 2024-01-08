import argparse
import os.path

parser = argparse.ArgumentParser()
parser.add_argument('--init', action='store_true', help='Init Project')
parser.add_argument('--config', action='store_true', help='Config Project')
parser.add_argument('--project', help='project path')
parser.add_argument('--engine', help='engine path')


def main():
    args = parser.parse_args()
    project_path = args.project
    engine_path = args.engine
    print("Init Project... \nProject Path: %s\nEngine  Path: %s" % (project_path, engine_path))

    if args.init:
        # check project path exists
        assert os.path.exists(project_path)

        project_link = os.path.join(engine_path, "active_project")
        engine_asset_path = os.path.join(engine_path, "assets")
        engine_asset_link = os.path.join(project_path, "engine_assets")

        # create project link
        if os.path.exists(project_link):
            os.rmdir(project_link)
        os.symlink(project_path, project_link, True)

        # engine asset dir link
        if os.path.exists(engine_asset_link):
            os.rmdir(engine_asset_link)
        os.symlink(engine_asset_path, engine_asset_link)


if __name__ == '__main__':
    main()

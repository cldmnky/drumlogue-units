# Drumlogue Units Documentation Site

This directory contains the GitHub Pages site for the drumlogue-units project.

## Structure

```text
docs/
├── _config.yml          # Jekyll configuration
├── _layouts/            # HTML templates
│   ├── default.html     # Base layout
│   └── unit.html        # Unit page layout
├── assets/
│   └── css/
│       └── style.css    # Site stylesheet
├── index.md             # Home page
├── installation/
│   └── index.md         # Installation guide
└── units/
    ├── index.md         # Units listing
    ├── clouds-revfx.md  # Clouds Reverb FX page
    └── elementish-synth.md  # Elementish Synth page
```

## Deployment

The site is automatically deployed to GitHub Pages when changes are pushed to the `main` branch.

To enable GitHub Pages:

1. Go to Settings → Pages in your GitHub repository
2. Under "Build and deployment", select **GitHub Actions** as the source
3. The workflow will automatically build and deploy on push

The site will be available at: <https://cldmnky.github.io/drumlogue-units/>

## Local Development (Optional)

### Option 1: Docker (Recommended)

```bash
cd docs
docker run --rm -it -p 4000:4000 -v $(pwd):/srv/jekyll jekyll/jekyll:4 jekyll serve --host 0.0.0.0
```

### Option 2: Install Ruby via Homebrew (macOS)

The system Ruby on macOS is outdated. Install a modern Ruby:

```bash
# Install rbenv and ruby-build
brew install rbenv ruby-build

# Install Ruby 3.2
rbenv install 3.2.2
rbenv global 3.2.2

# Add to your shell profile (~/.zshrc or ~/.bash_profile)
echo 'eval "$(rbenv init -)"' >> ~/.zshrc
source ~/.zshrc

# Now install Jekyll
cd docs
bundle install
bundle exec jekyll serve
```

Then open <http://localhost:4000/drumlogue-units/>

## Adding New Units

To add a new unit to the documentation:

1. Create a new file in `docs/units/` named `<unit-name>.md`
2. Use the `unit` layout with appropriate front matter:

   ```yaml
   ---
   layout: unit
   title: Unit Name
   tagline: Brief description of the unit
   unit_type: Synthesizer  # or "Reverb Effect", "Delay Effect", etc.
   version: v1.0.0
   filename: unit_name.drmlgunit
   download_url: https://github.com/cldmnky/drumlogue-units/releases
   ---
   ```

3. Add unit content following the structure of existing unit pages
4. Update `docs/units/index.md` to include the new unit in the listing
5. Update `docs/index.md` to add the unit card (if desired)

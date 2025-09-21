// Icon System for Embedded UR WebIF API
// Uses Heroicons from CDN (https://heroicons.com/)

class IconLoader {
    constructor() {
        this.cache = new Map();
        this.iconBasePath = 'https://unpkg.com/heroicons@2.1.1/24/outline/';
        this.iconSolidPath = 'https://unpkg.com/heroicons@2.1.1/24/solid/';
        this.loadedIcons = new Set();
    }

    async loadIcon(iconName, variant = 'outline') {
        const cacheKey = `${iconName}-${variant}`;
        if (this.cache.has(cacheKey)) {
            return this.cache.get(cacheKey);
        }

        try {
            const basePath = variant === 'solid' ? this.iconSolidPath : this.iconBasePath;
            const mappedName = this.mapIconName(iconName);
            const response = await fetch(`${basePath}${mappedName}.svg`);
            if (!response.ok) {
                throw new Error(`Failed to load icon: ${iconName}`);
            }

            const svgContent = await response.text();
            this.cache.set(cacheKey, svgContent);
            return svgContent;
        } catch (error) {
            console.warn(`Failed to load icon ${iconName}:`, error);
            return this.getFallbackIcon(iconName);
        }
    }

    // Map local icon names to Heroicons names
    mapIconName(iconName) {
        const iconMap = {
            'lock': 'lock-closed',
            'user': 'user',
            'eye': 'eye',
            'eye-slash': 'eye-slash',
            'cloud-arrow-up': 'arrow-up-on-square',
            'upload': 'arrow-up-tray',
            'document': 'document',
            'x-mark': 'x-mark',
            'spinner': 'arrow-path',
            'shield-check': 'shield-check',
            'shield': 'shield-exclamation',
            'chevron-down': 'chevron-down',
            'favicon': 'shield-check'
        };
        return iconMap[iconName] || iconName;
    }

    getFallbackIcon(iconName) {
        // Simple fallback icons as minimal SVGs
        const fallbacks = {
            'lock': '<svg viewBox="0 0 24 24" fill="currentColor"><rect x="6" y="10" width="12" height="8" rx="2"/><path d="M8 10V8a4 4 0 118 0v2"/></svg>',
            'user': '<svg viewBox="0 0 24 24" fill="currentColor"><circle cx="12" cy="8" r="4"/><path d="M20 20c0-4.4-3.6-8-8-8s-8 3.6-8 8"/></svg>',
            'eye': '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M12 4.5C7 4.5 2.7 7.6 1 12c1.7 4.4 6 7.5 11 7.5s9.3-3.1 11-7.5c-1.7-4.4-6-7.5-11-7.5zm0 12c-2.5 0-4.5-2-4.5-4.5s2-4.5 4.5-4.5 4.5 2 4.5 4.5-2 4.5-4.5 4.5zm0-7c-1.4 0-2.5 1.1-2.5 2.5s1.1 2.5 2.5 2.5 2.5-1.1 2.5-2.5-1.1-2.5-2.5-2.5z"/></svg>',
            'eye-slash': '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M12 7c2.8 0 5 2.2 5 5 0 .6-.1 1.2-.3 1.7l2.9 2.9c1.5-1.3 2.7-2.9 3.4-4.6-1.7-4.4-6-7.5-11-7.5-1.4 0-2.8.3-4 .7l2.1 2.1C10.8 7.1 11.4 7 12 7zM2 4.3l2.3 2.3c-1.6 1.4-2.9 3.2-3.3 5.4 1.7 4.4 6 7.5 11 7.5 1.7 0 3.4-.4 4.9-1l2.3 2.3 1.4-1.4L3.4 2.9 2 4.3zM7 9.2l1.5 1.5c-.1.4-.2.9-.2 1.3 0 2.8 2.2 5 5 5 .5 0 .9-.1 1.3-.2l1.5 1.5c-.8.3-1.8.5-2.8.5-2.8 0-5-2.2-5-5 0-1 .2-2 .7-2.6z"/></svg>',
            'cloud-arrow-up': '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M19.4 13c.7 0 1.3-.6 1.3-1.3 0-4.6-3.7-8.4-8.3-8.4S4 7.1 4 11.7c-1.6 0-3 1.3-3 2.9s1.4 2.9 3 2.9h4v-3l-1.5 1.5-1.4-1.4L12 7.8l6.9 6.9-1.4 1.4L16 14.6v3h3.4z"/></svg>',
            'document': '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M6 2h8l6 6v12c0 1.1-.9 2-2 2H6c-1.1 0-2-.9-2-2V4c0-1.1.9-2 2-2zm7 7V3.5L18.5 9H13z"/></svg>',
            'x-mark': '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M18.3 5.7L12 12l6.3 6.3-1.4 1.4L10.6 13.4 4.3 19.7 2.9 18.3 9.2 12 2.9 5.7l1.4-1.4 6.3 6.3 6.3-6.3z"/></svg>',
            'spinner': '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" class="animate-spin"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"/></svg>',
            'shield-check': '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M12 2L4 6v6c0 5.5 3.8 10.7 9 12 5.2-1.3 9-6.5 9-12V6l-8-4zM10 17l-4-4 1.4-1.4L10 14.2l6.6-6.6L18 9l-8 8z"/></svg>',
            'chevron-down': '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M19 9l-7 7-7-7"/></svg>'
        };

        return fallbacks[iconName] || '<svg viewBox="0 0 24 24" fill="currentColor"><rect width="24" height="24"/></svg>';
    }

    async loadAllIcons() {
        const iconNames = ['lock', 'user', 'eye', 'eye-slash', 'cloud-arrow-up', 'document', 'x-mark', 'spinner', 'shield-check', 'chevron-down'];

        for (const iconName of iconNames) {
            try {
                await this.loadIcon(iconName);
                this.loadedIcons.add(iconName);
            } catch (error) {
                console.warn(`Failed to preload icon: ${iconName}`);
            }
        }
    }

    async replaceIconElements() {
        const iconElements = document.querySelectorAll('.icon[class*="icon-"]');

        for (const element of iconElements) {
            const classList = Array.from(element.classList);
            const iconClass = classList.find(cls => cls.startsWith('icon-') && cls !== 'icon');

            if (iconClass) {
                const iconName = iconClass.replace('icon-', '');
                // Check for solid variant class
                const variant = classList.includes('icon-solid') ? 'solid' : 'outline';
                const svgContent = await this.loadIcon(iconName, variant);

                // Create a container that maintains all original classes
                const newElement = document.createElement('span');
                newElement.className = element.className;
                newElement.innerHTML = svgContent;

                // Copy any inline styles
                if (element.style.cssText) {
                    newElement.style.cssText = element.style.cssText;
                }

                // Replace the element
                element.parentNode.replaceChild(newElement, element);
            }
        }
    }

    // Method to create icon elements programmatically
    createIcon(iconName, className = '', variant = 'outline') {
        const span = document.createElement('span');
        span.className = `icon icon-${iconName} ${className}`.trim();

        // Load the icon asynchronously
        this.loadIcon(iconName, variant).then(svgContent => {
            span.innerHTML = svgContent;
        });

        return span;
    }
}

// Global icon loader instance
window.iconLoader = new IconLoader();

// Initialize icons when DOM is ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initializeIcons);
} else {
    initializeIcons();
}

async function initializeIcons() {
    try {
        await window.iconLoader.loadAllIcons();
        await window.iconLoader.replaceIconElements();
        console.log('Icons loaded successfully');
    } catch (error) {
        console.warn('Icon loading failed, using fallbacks:', error);
    }
}

// Functionality for expandable authentication field
function initializeExpandableAuthField() {
    const authFields = document.querySelectorAll('.auth-field-expandable');

    authFields.forEach(field => {
        const expandButton = field.querySelector('.expand-button');
        const fileArea = field.querySelector('.file-upload-area');

        if (expandButton && fileArea) {
            expandButton.addEventListener('click', () => {
                const isExpanded = fileArea.style.display === 'block';
                fileArea.style.display = isExpanded ? 'none' : 'block';

                // Toggle chevron icon
                const chevronIcon = expandButton.querySelector('.icon-chevron-down');
                if (chevronIcon) {
                    chevronIcon.style.transform = isExpanded ? 'rotate(0deg)' : 'rotate(180deg)';
                }
            });

            // Initially set to unexpanded state
            fileArea.style.display = 'none';
        }
    });
}

// Call the function to initialize expandable fields after icons are loaded
async function initializeApp() {
    try {
        await window.iconLoader.loadAllIcons();
        await window.iconLoader.replaceIconElements();
        console.log('Icons loaded successfully');
        initializeExpandableAuthField(); // Initialize the expandable field
    } catch (error) {
        console.warn('App initialization failed:', error);
    }
}

// Re-initialize everything when DOM is ready, including the expandable field logic
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initializeApp);
} else {
    initializeApp();
}


// Export for use in other scripts
if (typeof module !== 'undefined' && module.exports) {
    module.exports = IconLoader;
}
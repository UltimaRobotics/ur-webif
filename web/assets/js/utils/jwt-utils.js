// JWT Utilities for Session Management
// Lightweight JWT implementation for frontend session handling

class JWTUtils {
    constructor() {
        console.log('[JWT-UTILS] JWT utilities initialized');
    }

    // Base64 URL-safe encoding
    base64UrlEncode(str) {
        const base64 = btoa(str);
        return base64.replace(/\+/g, '-').replace(/\//g, '_').replace(/=/g, '');
    }

    // Base64 URL-safe decoding
    base64UrlDecode(str) {
        // Add padding if needed
        str += '==='.slice(0, (4 - str.length % 4) % 4);
        str = str.replace(/-/g, '+').replace(/_/g, '/');
        return atob(str);
    }

    // Parse JWT token (without signature verification for client-side)
    parseJWT(token) {
        try {
            if (!token || typeof token !== 'string') {
                console.warn('[JWT-UTILS] Invalid token provided');
                return null;
            }

            const parts = token.split('.');
            if (parts.length !== 3) {
                console.warn('[JWT-UTILS] Invalid JWT format');
                return null;
            }

            const header = JSON.parse(this.base64UrlDecode(parts[0]));
            const payload = JSON.parse(this.base64UrlDecode(parts[1]));
            const signature = parts[2];

            console.log('[JWT-UTILS] JWT parsed successfully');
            return {
                header,
                payload,
                signature,
                raw: token
            };
        } catch (error) {
            console.error('[JWT-UTILS] Error parsing JWT:', error);
            return null;
        }
    }

    // Check if JWT token is expired
    isTokenExpired(token) {
        const jwt = this.parseJWT(token);
        if (!jwt || !jwt.payload.exp) {
            console.warn('[JWT-UTILS] No expiration time in token');
            return true;
        }

        const currentTime = Math.floor(Date.now() / 1000);
        const isExpired = jwt.payload.exp < currentTime;
        
        if (isExpired) {
            console.log('[JWT-UTILS] Token is expired');
        }
        
        return isExpired;
    }

    // Get token expiration time
    getTokenExpiration(token) {
        const jwt = this.parseJWT(token);
        if (!jwt || !jwt.payload.exp) {
            return null;
        }
        return new Date(jwt.payload.exp * 1000);
    }

    // Get user info from JWT payload
    getUserInfo(token) {
        const jwt = this.parseJWT(token);
        if (!jwt) {
            return null;
        }

        return {
            username: jwt.payload.username || jwt.payload.sub,
            role: jwt.payload.role || 'user',
            exp: jwt.payload.exp,
            iat: jwt.payload.iat,
            sessionId: jwt.payload.sessionId || jwt.payload.jti
        };
    }

    // Create a simple JWT token (for testing - in production this should be done server-side)
    createJWT(payload, secret = 'ur-webif-secret') {
        const header = {
            alg: 'HS256',
            typ: 'JWT'
        };

        const encodedHeader = this.base64UrlEncode(JSON.stringify(header));
        const encodedPayload = this.base64UrlEncode(JSON.stringify(payload));
        
        // Simple signature (in production, use proper HMAC)
        const signature = this.base64UrlEncode(btoa(encodedHeader + '.' + encodedPayload + secret));
        
        return `${encodedHeader}.${encodedPayload}.${signature}`;
    }

    // Convert regular session token to JWT format
    convertSessionToJWT(sessionToken, username, role = 'user') {
        const now = Math.floor(Date.now() / 1000);
        const payload = {
            username: username,
            role: role,
            sessionId: sessionToken,
            iat: now,
            exp: now + (24 * 60 * 60), // 24 hours
            iss: 'ur-webif-api',
            aud: 'ur-webif-client'
        };

        return this.createJWT(payload);
    }

    // Validate JWT format and basic structure
    validateJWTFormat(token) {
        if (!token || typeof token !== 'string') {
            return false;
        }

        const parts = token.split('.');
        if (parts.length !== 3) {
            return false;
        }

        try {
            JSON.parse(this.base64UrlDecode(parts[0])); // header
            JSON.parse(this.base64UrlDecode(parts[1])); // payload
            return true;
        } catch (error) {
            console.error('[JWT-UTILS] JWT format validation failed:', error);
            return false;
        }
    }

    // Get time until token expiration in seconds
    getTimeUntilExpiration(token) {
        const jwt = this.parseJWT(token);
        if (!jwt || !jwt.payload.exp) {
            return 0;
        }

        const currentTime = Math.floor(Date.now() / 1000);
        const timeUntilExp = jwt.payload.exp - currentTime;
        return Math.max(0, timeUntilExp);
    }

    // Check if token needs refresh (expires within 5 minutes)
    needsRefresh(token, refreshThreshold = 300) { // 5 minutes
        const timeUntilExp = this.getTimeUntilExpiration(token);
        return timeUntilExp > 0 && timeUntilExp < refreshThreshold;
    }
}

// Create global instance
window.jwtUtils = new JWTUtils();

console.log('[JWT-UTILS] JWT utilities loaded and available globally');
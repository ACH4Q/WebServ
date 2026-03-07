(function () {
    function initStarfield() {
        var starCount = 26;
        var isHomePage = !!document.querySelector('.home-page');
        var movementStrength = isHomePage ? 28 : 18;
        var followEase = isHomePage ? 0.1 : 0.08;
        var starfield = document.createElement('div');
        starfield.className = 'starfield';
        document.body.appendChild(starfield);

        var stars = [];
        var targetX = window.innerWidth * 0.5;
        var targetY = window.innerHeight * 0.5;
        var pointerX = targetX;
        var pointerY = targetY;

        for (var index = 0; index < starCount; index++) {
            var star = document.createElement('span');
            star.className = 'star';

            var randomSize = Math.random();
            if (randomSize < 0.25) {
                star.classList.add('small');
            } else if (randomSize > 0.82) {
                star.classList.add('large');
            }

            var baseX = Math.random() * window.innerWidth;
            var baseY = Math.random() * window.innerHeight;
            var depth = 0.2 + Math.random() * 0.9;

            star.style.left = baseX + 'px';
            star.style.top = baseY + 'px';
            star.style.animationDelay = (Math.random() * 4).toFixed(2) + 's';
            star.style.animationDuration = (3.5 + Math.random() * 3.8).toFixed(2) + 's';

            starfield.appendChild(star);
            stars.push({
                element: star,
                baseX: baseX,
                baseY: baseY,
                depth: depth
            });
        }

        function render() {
            targetX += (pointerX - targetX) * followEase;
            targetY += (pointerY - targetY) * followEase;

            var offsetX = ((targetX / window.innerWidth) - 0.5) * movementStrength;
            var offsetY = ((targetY / window.innerHeight) - 0.5) * movementStrength;

            for (var i = 0; i < stars.length; i++) {
                var item = stars[i];
                var dx = offsetX * item.depth;
                var dy = offsetY * item.depth;
                item.element.style.transform = 'translate(' + dx.toFixed(2) + 'px, ' + dy.toFixed(2) + 'px)';
            }

            requestAnimationFrame(render);
        }

        window.addEventListener('mousemove', function (event) {
            pointerX = event.clientX;
            pointerY = event.clientY;
        });

        window.addEventListener('touchmove', function (event) {
            if (event.touches && event.touches.length > 0) {
                pointerX = event.touches[0].clientX;
                pointerY = event.touches[0].clientY;
            }
        }, { passive: true });

        window.addEventListener('resize', function () {
            for (var i = 0; i < stars.length; i++) {
                var item = stars[i];
                item.baseX = Math.random() * window.innerWidth;
                item.baseY = Math.random() * window.innerHeight;
                item.element.style.left = item.baseX + 'px';
                item.element.style.top = item.baseY + 'px';
            }
        });

        render();
    }

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', initStarfield);
    } else {
        initStarfield();
    }
})();

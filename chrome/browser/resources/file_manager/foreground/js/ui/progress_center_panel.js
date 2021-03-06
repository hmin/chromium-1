// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Item element of the progress center.
 * @param {HTMLDocument} document Document which the new item belongs to.
 * @constructor
 */
function ProgressCenterItemElement(document) {
  var label = document.createElement('label');
  label.className = 'label';

  var progressBarIndicator = document.createElement('div');
  progressBarIndicator.className = 'progress-track';

  var progressBar = document.createElement('div');
  progressBar.className = 'progress-bar';
  progressBar.appendChild(progressBarIndicator);

  var cancelButton = document.createElement('button');
  cancelButton.className = 'cancel';
  cancelButton.setAttribute('tabindex', '-1');

  var progressFrame = document.createElement('div');
  progressFrame.className = 'progress-frame';
  progressFrame.appendChild(progressBar);
  progressFrame.appendChild(cancelButton);

  var itemElement = document.createElement('li');
  itemElement.appendChild(label);
  itemElement.appendChild(progressFrame);

  return ProgressCenterItemElement.decorate(itemElement);
}

/**
 * Event triggered when the item should be dismissed.
 * @type {string}
 * @const
 */
ProgressCenterItemElement.PROGRESS_ANIMATION_END_EVENT = 'progressAnimationEnd';

/**
 * Decoreates the given element as a progress item.
 * @param {HTMLElement} element Item to be decoreated.
 * @return {ProgressCenterItemElement} Decoreated item.
 */
ProgressCenterItemElement.decorate = function(element) {
  element.__proto__ = ProgressCenterItemElement.prototype;
  element.state_ = ProgressItemState.PROGRESSING;
  element.timeoutId_ = null;
  element.track_ = element.querySelector('.progress-track');
  element.track_.addEventListener('webkitTransitionEnd',
                                  element.onTransitionEnd_.bind(element));
  element.nextWidthRate_ = null;
  element.timeoutId_ = null;
  return element;
};

ProgressCenterItemElement.prototype = {
  __proto__: HTMLDivElement.prototype,
  get animated() {
    return !!(this.timeoutId_ || this.track_.classList.contains('animated'));
  },
  get quiet() {
    return this.classList.contains('quiet');
  }
};

/**
 * Updates the element view according to the item.
 * @param {ProgressCenterItem} item Item to be referred for the update.
 * @param {boolean} animated Whether the progress width is applied as animated
 *     or not.
 */
ProgressCenterItemElement.prototype.update = function(item, animated) {
  // Set element attributes.
  this.state_ = item.state;
  this.setAttribute('data-progress-id', item.id);
  this.classList.toggle('error', item.state === ProgressItemState.ERROR);
  this.classList.toggle('cancelable', item.cancelable);
  this.classList.toggle('single', item.single);
  this.classList.toggle('quiet', item.quiet);

  // Set label.
  if (this.state_ === ProgressItemState.PROGRESSING ||
      this.state_ === ProgressItemState.ERROR) {
    this.querySelector('label').textContent = item.message;
  } else if (this.state_ === ProgressItemState.CANCELED) {
    this.querySelector('label').textContent = '';
  }

  // Set track width.
  this.nextWidthRate_ = item.progressRateInPercent;
  var setWidth = function() {
    this.timeoutId_ = null;
    var currentWidthRate = parseInt(this.track_.style.width);
    // Prevent assigning the same width to avoid stopping the animation.
    // animated == false may be intended to cancel the animation, so in that
    // case, the assignment should be done.
    if (currentWidthRate === this.nextWidthRate_ && animated)
      return;
    this.track_.hidden = false;
    this.track_.style.width = this.nextWidthRate_ + '%';
    this.track_.classList.toggle('animated', animated);
  }.bind(this);
  if (animated) {
    this.timeoutId_ = this.timeoutId_ || setTimeout(setWidth);
  } else {
    clearTimeout(this.timeoutId_);
    // For animated === false, we should call setWidth immediately to cancel the
    // animation, otherwise the animation may complete before canceling it.
    setWidth();
  }
};

/**
 * Resets the item.
 */
ProgressCenterItemElement.prototype.reset = function() {
  this.track_.hidden = true;
  this.track_.width = '';
  this.state_ = ProgressItemState.PROGRESSING;
};

/**
 * Handles transition end events.
 * @param {Event} event Transition end event.
 * @private
 */
ProgressCenterItemElement.prototype.onTransitionEnd_ = function(event) {
  if (event.propertyName !== 'width')
    return;
  this.track_.classList.remove('animated');
  this.dispatchEvent(new Event(
      ProgressCenterItemElement.PROGRESS_ANIMATION_END_EVENT,
      {bubbles: true}));
};

/**
 * Progress center panel.
 *
 * @param {HTMLElement} element DOM Element of the process center panel.
 * @constructor
 */
function ProgressCenterPanel(element) {
  /**
   * Root element of the progress center.
   * @type {HTMLElement}
   * @private
   */
  this.element_ = element;

  /**
   * Open view containing multiple progress items.
   * @type {HTMLElement}
   * @private
   */
  this.openView_ = this.element_.querySelector('#progress-center-open-view');

  /**
   * Close view that is a summarized progress item.
   * @type {HTMLElement}
   * @private
   */
  this.closeView_ = ProgressCenterItemElement.decorate(
      this.element_.querySelector('#progress-center-close-view'));

  /**
   * Toggle animation rule of the progress center.
   * @type {CSSKeyFrameRule}
   * @private
   */
  this.toggleAnimation_ = ProgressCenterPanel.getToggleAnimation_(
      element.ownerDocument);

  /**
   * Item group for normal priority items.
   * @type {ProgressCenterItemGroup}
   * @private
   */
  this.normalItemGroup_ = new ProgressCenterItemGroup('normal', false);

  /**
   * Item group for low priority items.
   * @type {ProgressCenterItemGroup}
   * @private
   */
  this.quietItemGroup_ = new ProgressCenterItemGroup('quiet', true);

  /**
   * Queries to obtains items for each group.
   * @type {Object.<string, string>}
   * @private
   */
  this.itemQuery_ = Object.seal({
    normal: 'li:not(.quiet)',
    quiet: 'li.quiet'
  });

  /**
   * Timeout IDs of the inactive state of each group.
   * @type {Object.<string, number?>}
   * @private
   */
  this.timeoutId_ = Object.seal({
    normal: null,
    quiet: null
  });

  /**
   * Callback to becalled with the ID of the progress item when the cancel
   * button is clicked.
   */
  this.cancelCallback = null;

  Object.seal(this);

  // Register event handlers.
  element.addEventListener('click', this.onClick_.bind(this));
  element.addEventListener(
      'webkitAnimationEnd', this.onToggleAnimationEnd_.bind(this));
  element.addEventListener(
      ProgressCenterItemElement.PROGRESS_ANIMATION_END_EVENT,
      this.onItemAnimationEnd_.bind(this));
}

/**
 * Obtains the toggle animation keyframes rule from the document.
 * @param {HTMLDocument} document Document containing the rule.
 * @return {CSSKeyFrameRules} Animation rule.
 * @private
 */
ProgressCenterPanel.getToggleAnimation_ = function(document) {
  for (var i = 0; i < document.styleSheets.length; i++) {
    var styleSheet = document.styleSheets[i];
    for (var j = 0; j < styleSheet.cssRules.length; j++) {
      var rule = styleSheet.cssRules[j];
      if (rule.type === CSSRule.WEBKIT_KEYFRAMES_RULE &&
          rule.name === 'progress-center-toggle') {
        return rule;
      }
    }
  }
  throw new Error('The progress-center-toggle rules is not found.');
};

/**
 * The default amount of milliseconds time, before a progress item will reset
 * after the last complete.
 * @type {number}
 * @private
 * @const
 */
ProgressCenterPanel.RESET_DELAY_TIME_MS_ = 5000;

/**
 * Updates an item to the progress center panel.
 * @param {!ProgressCenterItem} item Item including new contents.
 */
ProgressCenterPanel.prototype.updateItem = function(item) {
  var targetGroup = this.getGroupForItem_(item);

  // Update the item.
  var oldState = targetGroup.state;
  targetGroup.update(item);
  this.handleGroupStateChange_(targetGroup, oldState, targetGroup.state);

  // Update an open view item.
  var newItem = targetGroup.getItem(item.id);
  var itemElement = this.getItemElement_(item.id);
  if (newItem) {
    if (!itemElement) {
      itemElement = new ProgressCenterItemElement(this.element_.ownerDocument);
      this.openView_.insertBefore(itemElement, this.openView_.firstNode);
    }
    itemElement.update(newItem, targetGroup.isAnimated(item.id));
  } else {
    if (itemElement)
      itemElement.parentNode.removeChild(itemElement);
  }

  // Update the close view.
  this.updateCloseView_();
};

/**
 * Handles the item animation end.
 * @param {Event} event Item animation end event.
 * @private
 */
ProgressCenterPanel.prototype.onItemAnimationEnd_ = function(event) {
  var targetGroup = event.target.classList.contains('quiet') ?
      this.quietItemGroup_ : this.normalItemGroup_;
  var oldState = targetGroup.state;
  if (event.target === this.closeView_) {
    targetGroup.completeSummarizedItemAnimation();
  } else {
    var itemId = event.target.getAttribute('data-progress-id');
    targetGroup.completeItemAnimation(itemId);
    var newItem = targetGroup.getItem(itemId);
    var itemElement = this.getItemElement_(itemId);
    if (!newItem && itemElement)
      itemElement.parentNode.removeChild(itemElement);
  }
  this.handleGroupStateChange_(targetGroup, oldState, targetGroup.state);
  this.updateCloseView_();
};

/**
 * Handles the state change of group.
 * @param {ProgressCenterItemGroup} group Item group.
 * @param {ProgressCenterItemGroup.State} oldState Old state of the group.
 * @param {ProgressCenterItemGroup.State} newState New state of the group.
 * @private
 */
ProgressCenterPanel.prototype.handleGroupStateChange_ =
    function(group, oldState, newState) {
  if (oldState === ProgressCenterItemGroup.State.INACTIVE) {
    clearTimeout(this.timeoutId_[group.name]);
    this.timeoutId_[group.name] = null;
    var elements =
        this.openView_.querySelectorAll(this.itemQuery_[group.name]);
    for (var i = 0; i < elements.length; i++) {
      elements[i].parentNode.removeChild(elements[i]);
    }
  }
  if (newState === ProgressCenterItemGroup.State.INACTIVE) {
    this.timeoutId_[group.name] = setTimeout(function() {
      var inOldState = group.state;
      group.endInactive();
      this.handleGroupStateChange_(group, inOldState, group.state);
      this.updateCloseView_();
    }.bind(this), ProgressCenterPanel.RESET_DELAY_TIME_MS_);
  }
};

/**
 * Updates the close view.
 * @private
 */
ProgressCenterPanel.prototype.updateCloseView_ = function() {
  // Try to use the normal summarized item.
  var normalSummarizedItem =
      this.normalItemGroup_.getSummarizedItem(this.quietItemGroup_.numErrors);
  if (normalSummarizedItem) {
    // If the quiet animation is overrided by normal summarized item, discard
    // the quiet animation.
    if (this.quietItemGroup_.isSummarizedAnimated()) {
      var oldState = this.quietItemGroup_.state;
      this.quietItemGroup_.completeSummarizedItemAnimation();
      this.handleGroupStateChange_(this.quietItemGroup_,
                                   oldState,
                                   this.quietItemGroup_.state);
    }

    // Update the view state.
    this.closeView_.update(normalSummarizedItem,
                           this.normalItemGroup_.isSummarizedAnimated());
    this.element_.hidden = false;
    return;
  }

  // Try to use the quiet summarized item.
  var quietSummarizedItem =
      this.quietItemGroup_.getSummarizedItem(this.normalItemGroup_.numErrors);
  if (quietSummarizedItem) {
    this.closeView_.update(quietSummarizedItem,
                           this.quietItemGroup_.isSummarizedAnimated());
    this.element_.hidden = false;
    return;
  }

  // Try to use the error summarized item.
  var errorSummarizedItem = ProgressCenterItemGroup.getSummarizedErrorItem(
      this.normalItemGroup_, this.quietItemGroup_);
  if (errorSummarizedItem) {
    this.closeView_.update(errorSummarizedItem, false);
    this.element_.hidden = false;
    return;
  }

  // Hide the progress center because there is no items to show.
  this.closeView_.reset();
  this.element_.hidden = true;
  this.element_.classList.remove('opened');
};

/**
 * Gets an item element having the specified ID.
 * @param {string} id progress item ID.
 * @return {HTMLElement} Item element having the ID.
 * @private
 */
ProgressCenterPanel.prototype.getItemElement_ = function(id) {
  var query = 'li[data-progress-id="' + id + '"]';
  return this.openView_.querySelector(query);
};

/**
 * Obtains the group for the item.
 * @param {ProgressCenterItem} item Progress item.
 * @return {ProgressCenterItemGroup} Item group that should contain the item.
 * @private
 */
ProgressCenterPanel.prototype.getGroupForItem_ = function(item) {
  return item.quiet ? this.quietItemGroup_ : this.normalItemGroup_;
};

/**
 * Handles the animation end event of the progress center.
 * @param {Event} event Animation end event.
 * @private
 */
ProgressCenterPanel.prototype.onToggleAnimationEnd_ = function(event) {
  // Transition end of the root element's height.
  if (event.target === this.element_ &&
      event.animationName === 'progress-center-toggle') {
    this.element_.classList.remove('animated');
    return;
  }
};

/**
 * Handles the click event.
 * @param {Event} event Click event.
 * @private
 */
ProgressCenterPanel.prototype.onClick_ = function(event) {
  // Toggle button.
  if (event.target.classList.contains('toggle') &&
      (!this.closeView_.classList.contains('single') ||
       this.element_.classList.contains('opened'))) {

    // If the progress center has already animated, just return.
    if (this.element_.classList.contains('animated'))
      return;

    // Obtains current and target height.
    var currentHeight;
    var targetHeight;
    if (this.element_.classList.contains('opened')) {
      currentHeight = this.openView_.getBoundingClientRect().height;
      targetHeight = this.closeView_.getBoundingClientRect().height;
    } else {
      currentHeight = this.closeView_.getBoundingClientRect().height;
      targetHeight = this.openView_.getBoundingClientRect().height;
    }

    // Set styles for animation.
    this.toggleAnimation_.cssRules[0].style.height = currentHeight + 'px';
    this.toggleAnimation_.cssRules[1].style.height = targetHeight + 'px';
    this.element_.classList.add('animated');
    this.element_.classList.toggle('opened');
    return;
  }

  // Cancel button.
  if (this.cancelCallback) {
    var id = event.target.classList.contains('toggle') ?
        this.closeView_.getAttribute('data-progress-id') :
        event.target.parentNode.parentNode.getAttribute('data-progress-id');
    this.cancelCallback(id);
  }
};
